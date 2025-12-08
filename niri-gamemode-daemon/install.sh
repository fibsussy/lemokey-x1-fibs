#!/bin/bash
set -e

echo "Installing Niri GameMode Daemon..."
echo "=================================="
echo

# Build release binary
echo "→ Building release binary..."
cargo build --release

# Install to cargo bin
echo "→ Installing to ~/.cargo/bin/..."
cargo install --path .

# Create systemd directory
echo "→ Setting up systemd service..."
mkdir -p ~/.config/systemd/user/

# Copy service file
cp niri-gamemode-daemon.service ~/.config/systemd/user/

# Reload systemd
systemctl --user daemon-reload

echo
echo "✓ Installation complete!"
echo
echo "Next steps:"
echo "  1. Enable and start the service:"
echo "     systemctl --user enable --now niri-gamemode-daemon.service"
echo
echo "  2. Check the status:"
echo "     systemctl --user status niri-gamemode-daemon.service"
echo
echo "  3. View logs:"
echo "     journalctl --user -u niri-gamemode-daemon.service -f"
echo
echo "Optional: Set keyboard VID/PID (if you have multiple keyboards):"
echo "  nano ~/.config/systemd/user/niri-gamemode-daemon.service"
echo "  Then: systemctl --user daemon-reload && systemctl --user restart niri-gamemode-daemon.service"
echo
