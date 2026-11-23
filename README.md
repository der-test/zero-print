# Zero Print
Zero Print is a camera-to-thermal-printer app for the camera-equipped M5Stack CardputerZero. It supports the MX06 BLE printer and USB ESC/POS printers, with monochrome dithering. The CardputerZero-Lite does not include the IMX219 camera.

## AI Warning
This project is 100% AI coded! Please use with caution, as the AI may have introduced bugs or security issues.

## Use

- Use the arrow keys to select Camera or Settings; press Enter/OK to open.
- Camera shows a live preview. Enter/OK captures the current frame and opens the print preview.
- Enter/OK prints; Esc returns to the live camera view. Hold Esc to close the app.
- Settings selects MX06 BLE or USB ESC/POS and Floyd-Steinberg, Atkinson, or threshold dithering. The selected printer mode is shown in camera and print preview.

The live and captured images are rotated 180 degrees to match the camera orientation. Captured JPEGs are temporary and removed when the app exits. Printer and dither settings are saved under `~/.config/zero-print/`.

## Printers

MX06 printing uses BLE. USB printing supports ESC/POS raster printers such as the Epson TM-T88V through `/dev/usb/lp*`. The app user must have permission to access the selected printer device.

## Launch And Permissions

After installing the ARM64 `.deb`, launch Zero Print from APPLaunch. The Linux user running the app must have permission to access the camera and selected printer device.

## Build The Debian Package

The build requires CMake 3.31+, Ninja, `dpkg-deb`, and an AArch64 GNU/Linux cross-compiler. The build downloads the CardputerZero BSP, LVGL, and fmt when missing, so the first run needs network access.

```sh
cd app/zero-print
cmake --workflow --preset cp0-cross-package
```

The ARM64 `.deb` is written to `app/zero-print/dist/`. Copy it to CardputerZero and install it from its destination, for example:

```sh
sudo apt install --no-install-recommends /tmp/zero-print_<version>_rev1_arm64.deb
```

The root `LICENSE` applies to the project. Third-party asset terms are listed in `app/zero-print/assets/fonts/LICENSE.txt`.