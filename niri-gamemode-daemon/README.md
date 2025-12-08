# Niri GameMode Daemon for QMK

Automatically switches your QMK keyboard to game mode when you focus a gamescope window in Niri.

## How It Works

1. **QMK Firmware**: Added RAW HID support to receive commands from the host
2. **Rust Daemon**: Monitors `niri msg event-stream` for window focus changes
3. **Auto-Detection**: When `app_id == "gamescope"`, sends HID command to enter game mode
4. **Auto-Exit**: When you switch away, sends command to exit game mode

## Setup

### 1. Flash Updated QMK Firmware

The firmware now has RAW HID support enabled. Flash it to your keyboard:

```bash
cd /home/fib/Code/lemokey-x1-fibs
./flash.sh
```

### 2. Build the Daemon

```bash
cd niri-gamemode-daemon
cargo build --release
```

### 3. Find Your Keyboard's VID/PID (if needed)

The daemon will auto-detect QMK RAW HID devices. If you have multiple keyboards, you can specify yours:

```bash
lsusb | grep -i keychron
# Look for something like: Bus 001 Device 005: ID 3434:01f0 Keychron
#                                                   ^^^^:^^^^
#                                                   VID : PID
```

### 4. Run the Daemon

**Auto-detect mode** (finds first QMK RAW HID device):
```bash
./target/release/niri-gamemode-daemon
```

**Manual VID/PID** (if you have multiple keyboards):
```bash
KEYBOARD_VID=0x3434 KEYBOARD_PID=0x01f0 ./target/release/niri-gamemode-daemon
```

### 5. Test It

1. Run the daemon in a terminal
2. Launch a game through gamescope (e.g., via Steam)
3. Watch the terminal output - you should see:
   - "Detected gamescope window - enabling game mode"
   - Your keyboard should send F24 and enter SOCD mode
4. Switch away from the game
   - "Left gamescope window - disabling game mode"
   - Keyboard sends F23 and exits SOCD mode

## Running as a Service

To run automatically on login, create a systemd user service:

```bash
mkdir -p ~/.config/systemd/user/
cat > ~/.config/systemd/user/niri-gamemode-daemon.service <<EOF
[Unit]
Description=Niri GameMode Keyboard Daemon
After=graphical-session.target

[Service]
Type=simple
ExecStart=/home/fib/Code/lemokey-x1-fibs/niri-gamemode-daemon/target/release/niri-gamemode-daemon
Restart=on-failure
RestartSec=5

[Install]
WantedBy=default.target
EOF

# Enable and start
systemctl --user daemon-reload
systemctl --user enable --now niri-gamemode-daemon.service

# Check status
systemctl --user status niri-gamemode-daemon.service

# View logs
journalctl --user -u niri-gamemode-daemon.service -f
```

## Troubleshooting

### Permission Denied on HID Device

If you get permission errors accessing the keyboard:

```bash
# Add udev rule for your keyboard
echo 'KERNEL=="hidraw*", ATTRS{idVendor}=="3434", MODE="0666"' | sudo tee /etc/udev/rules.d/99-keychron.rules
sudo udevadm control --reload-rules
sudo udevadm trigger

# Or add yourself to input group (requires logout)
sudo usermod -a -G input $USER
```

### Daemon Can't Find Keyboard

- Make sure you flashed the updated firmware with `RAW_ENABLE = yes`
- Try running with sudo to rule out permissions: `sudo ./target/release/niri-gamemode-daemon`
- Check if the RAW HID interface exists: `ls -la /dev/hidraw*`

### Niri Command Not Found

Make sure `niri` is in your PATH and `niri msg event-stream` works:

```bash
niri msg event-stream
# Should output JSON events as you switch windows
```

## Protocol

The daemon uses these HID commands:

- `0x01`: Enter game mode
- `0x02`: Exit game mode
- `0x03`: Query status (not currently used)

Each command gets a 32-byte response with the first byte echoing the command.

## Extending

Want to trigger on different apps? Edit `src/main.rs` line 78:

```rust
let is_gamescope = app_id == Some("gamescope");
// Change to:
let is_gamescope = app_id == Some("gamescope") || app_id == Some("steam_game");
```

Or check window title instead:
```rust
let is_game = win.title.as_deref()
    .map(|t| t.contains("Sifu") || t.contains("Elden Ring"))
    .unwrap_or(false);
```
