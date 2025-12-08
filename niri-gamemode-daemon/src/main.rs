use hidapi::{HidApi, HidDevice};
use std::io::{BufRead, BufReader};
use std::process::{Command, Stdio};
use std::env;
use std::sync::mpsc::{self, Receiver, Sender, TryRecvError, RecvTimeoutError};
use std::thread;
use std::time::Duration;

// HID commands matching QMK firmware
const HID_CMD_ENTER_GAME_MODE: u8 = 0x01;
const HID_CMD_EXIT_GAME_MODE: u8 = 0x02;

// QMK RAW HID usage page and usage
const RAW_USAGE_PAGE: u16 = 0xFF60;
const RAW_USAGE: u16 = 0x61;

#[derive(Debug)]
enum DeviceEvent {
    Connected,
    Disconnected,
}

#[derive(Debug)]
enum NiriEvent {
    WindowFocusChanged(Option<String>),
}

struct GameModeController {
    device: HidDevice,
    current_state: bool, // true = in gamescope, false = not
}

impl GameModeController {
    fn new(device: HidDevice) -> Self {
        Self {
            device,
            current_state: false,
        }
    }

    fn send_command(&mut self, command: u8) -> Result<(), String> {
        // QMK RAW HID expects 32-byte packets
        let mut buf = [0u8; 32];
        buf[0] = command;

        println!("  → Sending: {:02X?}", &buf[..8]);

        let written = self.device
            .write(&buf)
            .map_err(|e| format!("Failed to send HID command: {}", e))?;

        println!("  → Wrote {} bytes", written);

        // Read response - try blocking read first, then with timeout
        let mut response = [0u8; 32];

        // First try: blocking read with 2 second timeout
        let n = self.device.read_timeout(&mut response, 2000)
            .map_err(|e| format!("Failed to read response: {}", e))?;

        println!("  ← Received {} bytes: {:02X?}", n, &response[..8]);

        if n == 0 {
            return Err("No response from keyboard (0 bytes received)".to_string());
        }

        if response[0] == command {
            println!("  ✓ Command 0x{:02X} acknowledged", command);
            Ok(())
        } else if response[0] == 0xFF {
            Err("Keyboard returned error (unknown command)".to_string())
        } else {
            Err(format!("Unexpected response: 0x{:02X} (expected 0x{:02X})", response[0], command))
        }
    }

    fn enter_game_mode(&mut self) -> Result<(), String> {
        println!("→ Entering game mode...");
        self.send_command(HID_CMD_ENTER_GAME_MODE)
    }

    fn exit_game_mode(&mut self) -> Result<(), String> {
        println!("← Exiting game mode...");
        self.send_command(HID_CMD_EXIT_GAME_MODE)
    }

    fn handle_window_change(&mut self, app_id: Option<&str>) -> Result<(), String> {
        let is_gamescope = app_id == Some("gamescope");

        if is_gamescope && !self.current_state {
            println!("\n🎮 Detected gamescope window!");
            self.enter_game_mode()?;
            self.current_state = true;
        } else if !is_gamescope && self.current_state {
            println!("\n💻 Left gamescope window");
            self.exit_game_mode()?;
            self.current_state = false;
        }

        Ok(())
    }
}

fn start_udev_monitor(tx: Sender<DeviceEvent>, vid: Option<u16>, pid: Option<u16>) -> Result<(), String> {
    thread::spawn(move || {
        loop {
            println!("✓ Starting udevadm monitor for hidraw events\n");

            let mut child = match Command::new("udevadm")
                .args(&["monitor", "--subsystem-match=hidraw"])
                .stdout(Stdio::piped())
                .spawn() {
                Ok(child) => child,
                Err(e) => {
                    eprintln!("Failed to spawn udevadm: {}", e);
                    eprintln!("Retrying in 5 seconds...");
                    thread::sleep(Duration::from_secs(5));
                    continue;
                }
            };

            let stdout = match child.stdout.take() {
                Some(stdout) => stdout,
                None => {
                    eprintln!("Failed to capture udevadm stdout");
                    thread::sleep(Duration::from_secs(5));
                    continue;
                }
            };

            let reader = BufReader::new(stdout);

            for line in reader.lines() {
                let line = match line {
                    Ok(line) => line,
                    Err(e) => {
                        eprintln!("Error reading udevadm output: {}", e);
                        break;
                    }
                };

                // Parse lines like: "KERNEL[12345.678] add      /devices/.../0003:362D:0210.003D/hidraw/hidraw1 (hidraw)"
                // or: "KERNEL[12345.678] remove   /devices/.../0003:362D:0210.003D/hidraw/hidraw0 (hidraw)"

                if !line.starts_with("KERNEL[") {
                    continue;
                }

                let is_add = line.contains("] add ");
                let is_remove = line.contains("] remove ");

                if !is_add && !is_remove {
                    continue;
                }

                // Extract VID:PID from path (format: 0003:VVVV:PPPP where 0003 is HID bus)
                let mut found_vid: Option<u16> = None;
                let mut found_pid: Option<u16> = None;

                for component in line.split('/') {
                    // Look specifically for HID device path component starting with "0003:"
                    if component.starts_with("0003:") && component.contains(':') {
                        let parts: Vec<&str> = component.split(':').collect();
                        if parts.len() >= 3 {
                            if let (Ok(v), Ok(p)) = (
                                u16::from_str_radix(parts[1], 16),
                                u16::from_str_radix(parts[2].split('.').next().unwrap_or(""), 16)
                            ) {
                                found_vid = Some(v);
                                found_pid = Some(p);
                                break;
                            }
                        }
                    }
                }

                if let (Some(device_vid), Some(device_pid)) = (found_vid, found_pid) {
                    // If VID/PID specified, only notify for matching devices
                    if let (Some(target_vid), Some(target_pid)) = (vid, pid) {
                        if device_vid != target_vid || device_pid != target_pid {
                            continue;
                        }
                    }

                    if is_add {
                        println!("[udev] HID device connected: {:04X}:{:04X}", device_vid, device_pid);
                        let _ = tx.send(DeviceEvent::Connected);
                    } else if is_remove {
                        println!("[udev] HID device disconnected: {:04X}:{:04X}", device_vid, device_pid);
                        let _ = tx.send(DeviceEvent::Disconnected);
                    }
                }
            }

            eprintln!("udevadm monitor process ended, restarting in 5 seconds...");
            thread::sleep(Duration::from_secs(5));
        }
    });

    Ok(())
}

fn find_keyboard(api: &HidApi, vid: Option<u16>, pid: Option<u16>) -> Result<HidDevice, String> {
    println!("Searching for keyboard...");

    // Scan for QMK RAW HID devices and open by path
    println!("Scanning for QMK RAW HID devices...");
    let mut found_device_info = None;

    for device in api.device_list() {
        // Check if VID/PID match (if specified)
        if let (Some(v), Some(p)) = (vid, pid) {
            if device.vendor_id() != v || device.product_id() != p {
                continue;
            }
        }

        // Check for RAW HID interface
        if device.usage_page() == RAW_USAGE_PAGE && device.usage() == RAW_USAGE {
            println!("Found QMK RAW HID device:");
            println!("  VID:0x{:04X} PID:0x{:04X}", device.vendor_id(), device.product_id());
            println!("  Manufacturer: {}", device.manufacturer_string().unwrap_or("Unknown"));
            println!("  Product: {}", device.product_string().unwrap_or("Unknown"));
            println!("  Path: {:?}", device.path());

            found_device_info = Some(device.path().to_owned());
            break;
        }
    }

    let device_path = found_device_info.ok_or("No QMK RAW HID devices found. Make sure your keyboard is connected and has RAW_ENABLE=yes")?;

    println!("Opening device by path...");
    api.open_path(&device_path)
        .map_err(|e| format!("Failed to open device: {}", e))
}

fn get_focused_window_app_id() -> Option<String> {
    let output = Command::new("niri")
        .args(&["msg", "focused-window"])
        .output()
        .ok()?;

    if !output.status.success() {
        return None;
    }

    let text = String::from_utf8(output.stdout).ok()?;

    // Parse the output looking for: App ID: "something"
    for line in text.lines() {
        if let Some(app_id_part) = line.trim().strip_prefix("App ID:") {
            // Extract string between quotes
            let app_id = app_id_part.trim().trim_matches('"');
            return Some(app_id.to_string());
        }
    }

    None
}

fn start_niri_monitor(tx: Sender<NiriEvent>) -> Result<(), String> {
    thread::spawn(move || {
        loop {
            println!("Starting niri event stream monitor...");
            println!("Watching for gamescope windows...\n");

            let mut child = match Command::new("niri")
                .args(&["msg", "event-stream"])
                .stdout(Stdio::piped())
                .spawn() {
                Ok(child) => child,
                Err(e) => {
                    eprintln!("Failed to spawn niri: {}", e);
                    thread::sleep(Duration::from_secs(5));
                    continue;
                }
            };

            let stdout = match child.stdout.take() {
                Some(stdout) => stdout,
                None => {
                    eprintln!("Failed to capture niri stdout");
                    thread::sleep(Duration::from_secs(5));
                    continue;
                }
            };

            let reader = BufReader::new(stdout);

            for line in reader.lines() {
                match line {
                    Ok(line) => {
                        if line.starts_with("Window focus changed:") {
                            let app_id = get_focused_window_app_id();
                            if tx.send(NiriEvent::WindowFocusChanged(app_id)).is_err() {
                                eprintln!("Niri monitor: channel closed, exiting");
                                return;
                            }
                        }
                    }
                    Err(e) => {
                        eprintln!("Error reading niri event: {}", e);
                        break;
                    }
                }
            }

            eprintln!("Niri event stream ended, restarting in 5 seconds...");
            thread::sleep(Duration::from_secs(5));
        }
    });

    Ok(())
}

fn main() {
    println!("Niri GameMode Daemon for QMK");
    println!("============================\n");

    // Parse VID/PID from environment
    let vid = env::var("KEYBOARD_VID")
        .ok()
        .and_then(|s| u16::from_str_radix(s.trim_start_matches("0x"), 16).ok());
    let pid = env::var("KEYBOARD_PID")
        .ok()
        .and_then(|s| u16::from_str_radix(s.trim_start_matches("0x"), 16).ok());

    if let (Some(v), Some(p)) = (vid, pid) {
        println!("Looking for keyboard VID:0x{:04X} PID:0x{:04X}\n", v, p);
    } else {
        println!("Searching for any QMK RAW HID keyboard");
        println!("To specify a keyboard, set: KEYBOARD_VID=0x362D KEYBOARD_PID=0x0210\n");
    }

    // Create channels for events
    let (device_tx, device_rx): (Sender<DeviceEvent>, Receiver<DeviceEvent>) = mpsc::channel();
    let (niri_tx, niri_rx): (Sender<NiriEvent>, Receiver<NiriEvent>) = mpsc::channel();

    // Start udev monitor in background thread
    if let Err(e) = start_udev_monitor(device_tx, vid, pid) {
        eprintln!("Warning: Failed to start udev monitor: {}", e);
        eprintln!("Will not receive hotplug events\n");
    }

    // Start niri monitor in background thread
    if let Err(e) = start_niri_monitor(niri_tx) {
        eprintln!("Fatal: Failed to start niri monitor: {}", e);
        std::process::exit(1);
    }

    let mut controller: Option<GameModeController> = None;

    // Try initial connection
    match HidApi::new() {
        Ok(api) => {
            if let Ok(device) = find_keyboard(&api, vid, pid) {
                println!("✓ Keyboard connected!\n");
                controller = Some(GameModeController::new(device));
            } else {
                println!("✗ Keyboard not found. Waiting for udev event...\n");
            }
        }
        Err(e) => {
            eprintln!("Failed to initialize HID API: {}", e);
        }
    }

    // Main event loop - purely event-driven, no polling
    loop {
        // Check for device events with short timeout so we can also check niri events
        match device_rx.recv_timeout(Duration::from_millis(50)) {
            Ok(DeviceEvent::Connected) => {
                println!("\n🔌 [udev] Device connected - attempting to connect...");

                // Drain immediate duplicates
                while let Ok(DeviceEvent::Connected) = device_rx.try_recv() {}

                // Rapidly poll until device is ready (up to 2 seconds)
                let start = std::time::Instant::now();
                let mut connected = false;

                while start.elapsed() < Duration::from_secs(2) {
                    if let Ok(api) = HidApi::new() {
                        if let Ok(device) = find_keyboard(&api, vid, pid) {
                            println!("✓ Keyboard connected!\n");
                            controller = Some(GameModeController::new(device));
                            connected = true;
                            break;
                        }
                    }

                    // Drain any more duplicate events that came in
                    while let Ok(DeviceEvent::Connected) = device_rx.try_recv() {}

                    thread::sleep(Duration::from_millis(10));
                }

                if !connected {
                    println!("✗ Failed to connect to keyboard after 2 seconds\n");
                }
            }
            Ok(DeviceEvent::Disconnected) => {
                println!("\n✗ [udev] Keyboard disconnected!");
                controller = None;

                // Drain any duplicate disconnect events that may follow
                thread::sleep(Duration::from_millis(100));
                while let Ok(DeviceEvent::Disconnected) = device_rx.try_recv() {
                    // Just consume duplicates
                }
            }
            Err(RecvTimeoutError::Disconnected) => {
                eprintln!("Device monitor died, exiting");
                break;
            }
            Err(RecvTimeoutError::Timeout) => {
                // Timeout, continue to check niri events
            }
        }

        // Process niri events if we have a keyboard
        if let Some(mut ctrl) = controller.take() {
            match niri_rx.try_recv() {
                Ok(NiriEvent::WindowFocusChanged(app_id)) => {
                    if let Some(ref app) = app_id {
                        println!("Focus changed → app_id: {}", app);
                    }

                    let result = ctrl.handle_window_change(app_id.as_deref());

                    if result.is_err() {
                        println!("\n✗ Device communication error - keyboard likely unplugged!");
                        println!("   Waiting for udev reconnect event...\n");
                        controller = None;
                        continue;
                    }

                    controller = Some(ctrl);
                }
                Err(TryRecvError::Disconnected) => {
                    eprintln!("Niri monitor died, exiting");
                    break;
                }
                Err(TryRecvError::Empty) => {
                    controller = Some(ctrl);
                }
            }
        }
    }
}
