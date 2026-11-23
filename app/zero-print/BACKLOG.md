# Follow-up

- USB ESC/POS output and permissions remain untested. The last CardputerZero check found no `/dev/usb/lp*` device; test when a USB thermal printer is connected.
- Monitor the IMX219 busy warning. If it recurs, inspect Zero Print/`rpicam-vid` processes read-only first; restart only after confirming a duplicate and getting approval.