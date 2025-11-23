#!/bin/bash

# Automated QMK compile and flash script
# Compiles the firmware and flashes it to the keyboard

sudo echo  # ensures password

BIN_FILE="keychron_x1_ansi_red_fibs.bin"
QMK_DIR="$HOME/Code/qmk_firmware"
KEYMAP_SOURCE="$PWD/keymaps/fibs"
QMK_KEYMAP_DEST="$QMK_DIR/keyboards/keychron/x1/ansi/red/keymaps/fibs"

echo "=== QMK Keyboard Compiler & Flasher ==="
echo ""

# Check if source files have changed
NEEDS_COMPILE=false

# Check if bin file exists in QMK directory
if [ ! -f "$QMK_DIR/$BIN_FILE" ]; then
    echo "Binary file not found, needs compilation"
    NEEDS_COMPILE=true
else
    # Check if any source files are newer than the bin file
    if [ -n "$(find "$KEYMAP_SOURCE" -type f -newer "$QMK_DIR/$BIN_FILE" 2>/dev/null)" ]; then
        echo "Source files have changed, needs recompilation"
        NEEDS_COMPILE=true
    else
        echo "Binary is up to date, skipping compilation"
    fi
fi

# Compile if needed
if [ "$NEEDS_COMPILE" = true ]; then
    echo ""
    echo "Copying keymap files to QMK directory..."
    mkdir -p "$QMK_KEYMAP_DEST"
    cp -r "$KEYMAP_SOURCE"/* "$QMK_KEYMAP_DEST/"

    echo "Compiling firmware..."
    cd "$QMK_DIR"
    qmk compile -kb keychron/x1/ansi/red -km fibs

    if [ $? -ne 0 ]; then
        echo ""
        echo "✗ Compilation failed!"
        exit 1
    fi

    echo ""
    echo "✓ Compilation successful!"
    cd - > /dev/null
fi

echo ""
echo "Waiting for keyboard to enter DFU mode..."
echo "(Put your keyboard into bootloader mode now)"
echo ""

# Wait for DFU device to appear
while true; do
    # Check if DFU device is available
    if lsusb | grep -qi "dfu"; then
        echo "DFU device detected!"
        sleep 0.5  # Give it a moment to fully initialize
        break
    fi
    sleep 0.5
done

echo "Flashing firmware..."
cd "$QMK_DIR"
sudo dfu-util -a 0 --dfuse-address 0x08000000:leave -D "$BIN_FILE"

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Firmware flashed successfully!"
else
    echo ""
    echo "✗ Flashing failed!"
    exit 1
fi
