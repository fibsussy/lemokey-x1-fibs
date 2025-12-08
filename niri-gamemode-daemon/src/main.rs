use hidapi::{HidApi, HidDevice};
use std::io::{BufRead, BufReader};
use std::process::{Command, Stdio};
use std::env;

// HID commands matching QMK firmware
const HID_CMD_ENTER_GAME_MODE: u8 = 0x01;
const HID_CMD_EXIT_GAME_MODE: u8 = 0x02;

// QMK RAW HID usage page and usage
const RAW_USAGE_PAGE: u16 = 0xFF60;
const RAW_USAGE: u16 = 0x61;

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

    fn handle_window_change(&mut self, app_id: Option<&str>) {
        let is_gamescope = app_id == Some("gamescope");

        if is_gamescope && !self.current_state {
            println!("\n🎮 Detected gamescope window!");
            if let Err(e) = self.enter_game_mode() {
                eprintln!("  ✗ Error entering game mode: {}", e);
            } else {
                self.current_state = true;
            }
        } else if !is_gamescope && self.current_state {
            println!("\n💻 Left gamescope window");
            if let Err(e) = self.exit_game_mode() {
                eprintln!("  ✗ Error exiting game mode: {}", e);
            } else {
                self.current_state = false;
            }
        }
    }
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

fn monitor_niri_events(mut controller: GameModeController) -> Result<(), String> {
    println!("Starting niri event stream monitor...");
    println!("Watching for gamescope windows...\n");

    let mut child = Command::new("niri")
        .args(&["msg", "event-stream"])
        .stdout(Stdio::piped())
        .spawn()
        .map_err(|e| format!("Failed to spawn niri: {}", e))?;

    let stdout = child.stdout.take()
        .ok_or("Failed to capture stdout")?;

    let reader = BufReader::new(stdout);

    for line in reader.lines() {
        let line = line.map_err(|e| format!("Error reading line: {}", e))?;

        // Look for window focus change events
        if line.starts_with("Window focus changed:") {
            // Query the focused window to get its app_id
            if let Some(app_id) = get_focused_window_app_id() {
                println!("Focus changed → app_id: {}", app_id);
                controller.handle_window_change(Some(&app_id));
            } else {
                // No focused window (e.g., on empty workspace)
                controller.handle_window_change(None);
            }
        }
    }

    Err("Event stream ended unexpectedly".to_string())
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

    // Initialize HID API
    let api = HidApi::new().expect("Failed to initialize HID API");

    // Find keyboard
    let device = match find_keyboard(&api, vid, pid) {
        Ok(dev) => dev,
        Err(e) => {
            eprintln!("Error: {}", e);
            eprintln!("\nTo specify your keyboard manually, set environment variables:");
            eprintln!("  KEYBOARD_VID=0x362D KEYBOARD_PID=0x0210 niri-gamemode-daemon");
            eprintln!("\nYou can find your keyboard's VID/PID with: lsusb");
            std::process::exit(1);
        }
    };

    let controller = GameModeController::new(device);

    // Start monitoring
    if let Err(e) = monitor_niri_events(controller) {
        eprintln!("Fatal error: {}", e);
        std::process::exit(1);
    }
}
