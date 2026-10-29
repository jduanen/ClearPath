# cpview — a Linux setup tool for ClearPath-SC motors

A Qt6 desktop app that covers the core ClearView workflow on Linux, built on
Teknic's sFoundation library (`../Linux_Software/sFoundation`).

**Works with:** ClearPath-SC motors (SCSK/SCHP) through an SC4-Hub.
**Does not work with:** ClearPath-MC/SD motors (use ClearPath MSP on Windows).

## Features
- **Toolbar:** Scan for SC hubs, Connect/Disconnect, Enable/Disable, and **E-STOP**
  (`Esc`). E-STOP sends an abrupt e-stop to every node; use *Clear Alerts* to resume.
- **Status tab:** model, type, serial number, firmware/hardware versions, resolution,
  and user ID. Shows status LEDs (ready, enabled, move done, alert, limits, inputs A/B…),
  live position/velocity/torque/bus voltage/temperature, alert and warning text, and a
  Clear Alerts button. New or changed alerts are also written to the log as errors.
- **Motion tab:** units (RPM or counts/s, % max or amps), press-and-hold jog, velocity
  moves, position moves (relative/absolute, dwell, repeat, back-and-forth), homing,
  and velocity/accel/jerk/dwell limits.
- **Config tab:** load/save `.mtr` config files (compatible with ClearView), restore
  factory defaults, user ID/description, torque/software/tracking limits, and a raw
  parameter table (every `CPM_P_*` register; double-click a value to write it).
  **Homing setup...** opens a ClearView-style dialog: direction, homing velocity and
  acceleration, target (input A or B turning on or off, or a hard stop with a torque
  limit), and the offset move's distance and direction.
- **Plot tab:** plot of position, velocity, torque, and tracking error, with CSV
  export and ClearView-style triggering: Auto, Normal, Single, Force, and Stop modes;
  triggers on the start or end of a move, a settled move, an alert, or a rising/falling
  slope through a level; trigger position 10/50/90%. Triggers are detected in software
  from the polled samples (see "Plot triggers" in the top-level README). The toolbar's
  **Plot Window** button moves the plot into its own window (`Esc` still E-stops there);
  closing that window or unchecking the button puts it back in its tab.

Not available, because sFoundation has no public API for them: auto-tune, firmware
update, and the high-speed hardware scope. The plot tab polls at 20 Hz.

## Setup
1. Serial permissions: `sudo usermod -aG dialout $USER`, then log out and back in.
2. For a USB-connected SC4-Hub, install the Exar driver
   (`../Linux_Software/Teknic_SC4Hub_USB_Driver/ExarKernelDriver/driver_readme.txt`).
   The hub then shows up as `/dev/ttyXRUSB0`.
   Whenever the hub reconnects, the generic `cdc_acm` driver claims it again (as
   `/dev/ttyACM0`, which sFoundation can't use). Install the udev rule so it's moved
   back automatically:
   `sudo cp 99-teknic-sc4hub.rules /etc/udev/rules.d/ && sudo udevadm control --reload`
3. Build sFoundation once: `cd ../Linux_Software/sFoundation/sFoundation && make`.
4. Install the build tools: `sudo apt install cmake qt6-base-dev qt6-charts-dev`.

## Build and run
```bash
cd /home/jdn/Code/ClearPath
cmake -S src -B build
cmake --build build -j
./build/cpview
```
Qt 6.10 needs CMake 3.22 or newer. If `cmake --version` shows an older one (for
example a stale `/usr/local/bin/cmake`), run `/usr/bin/cmake` instead.

If Connect fails with "Serial Port … Initialization problem" (0x80040601), the hub
was found but no motor network came up: check motor power, cabling, and the
SC4-Hub jumpers.
The app links to `libsFoundation20.so` where it was built (rpath), and the library loads
`MNuserDriver20.xml` from that same directory. CMake creates the `libsFoundation20.so.1`
soname link there if it is missing. To use another copy, pass
`-DSFND_ROOT=/path/to/sFoundation`.

Close ClearView and any SDK examples first; only one program can own the hub port.

## Code layout
- `MotorWorker.*`: every sFoundation call runs on one worker thread. It polls the
  selected node on a fixed 50 ms period (a poll takes ~25 ms on the wire) and emits
  a `Snapshot`.
- `MainWindow.*`: toolbar, node tree, tabs, and log.
- `tabs/*`: the four tabs and the Homing Setup dialog. They call the worker through `onWorker()` (queued).
- `CpmParams.inc`: generated from the `_cpmParams` enum in
  `inc/inc-pub/pubCpmRegs.h` (id, name, description).
