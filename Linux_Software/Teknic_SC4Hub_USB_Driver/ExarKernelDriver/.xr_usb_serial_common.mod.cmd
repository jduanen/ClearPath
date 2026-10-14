savedcmd_xr_usb_serial_common.mod := printf '%s\n'   xr_usb_serial_common.o | awk '!x[$$0]++ { print("./"$$0) }' > xr_usb_serial_common.mod
