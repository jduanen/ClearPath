# ClearPath

Tools and Documentation for Working with Teknic ClearPath Motors

This repo collects what's needed to set up and drive Teknic ClearPath-SC
(software-controlled) servo motors from Linux:

- the SC4-Hub USB driver, plus a udev rule that keeps it bound
- **cpview**, a Linux replacement for Teknic's Windows-only ClearView setup program
- Teknic's sFoundation C++ library and its example programs
- Teknic's manuals, API reference, CAD models, and Windows installers

It was developed against a **CPM-SCSK-2311S-RLSA** (NEMA 23, 800 counts/rev,
Advanced firmware) connected through an **SC4-Hub** over USB, on Ubuntu with Qt 6.10.

| Path | Contents |
|---|---|
| [`src/`](src/) | cpview source code, its CMake build, and the SC4-Hub udev rule |
| [`Linux_Software/`](Linux_Software/) | Teknic's Linux package: sFoundation, SDK examples, the Exar USB driver, and the API reference |
| [`docs/`](docs/) | Teknic user manuals and the sFoundation API reference |
| [`cad/`](cad/) | STEP models of the motor and SC4-Hub, plus the NEMA 23 dimensional drawing |
| [`bin/`](bin/) | Teknic's Windows installers (ClearView, ClearPath MSP, USB port driver) |

### Hardware overview

```
PC ──USB──▶ SC4-Hub ──comm cable──▶ ClearPath-SC motor(s)  (up to 4 per hub)
               ▲                          ▲
         24 VDC logic power        24–75 VDC bus power (separate supply)
```

The hub needs its own 24 VDC logic supply, and each motor needs DC bus power. Don't use
the bus supply for logic power, even if it's 24 V (*ClearPath-SC User Manual*, "Auxiliary
(24V) Logic Power Supply"). The hub also has a Global Stop input and two brake/general
purpose outputs.

I'm using the Teknic IPC-5 75VDC power supply unit for the motor bus power.

The motor part number decodes as follows (manual, Appendix H). `CPM-SCSK-2311S-RLSA`
means:

| Field | Value | Meaning |
|---|---|---|
| `SCSK` | model | Software control, "Stepper Killer" |
| `23` | frame | NEMA 23 |
| `1` | length | 82 mm body |
| `1` | winding | winding design 1 (see the torque-speed curve) |
| `S` | winding type | Series-Wye: more torque, lower speed |
| `R` | resolution | 800 counts/rev (`E` = 6400) |
| `L` | shaft | standard 3/8" shaft (`Q` = 1/4") |
| `S` | seal | extra Viton seal (`N` = standard dust seal) |
| `A` | features | Advanced firmware (`B` = Basic) |

## USB Driver Setup

The SC4-Hub (USB ID `2890:0213`) is an Exar USB-serial device. Linux's built-in
`cdc_acm` driver will claim it as `/dev/ttyACM0`, but Teknic says that driver "is NOT
fully compatible with sFoundation", and sFoundation's hub search only looks for
`/dev/ttyXRUSB*`. Install Teknic's Exar driver instead:

1. **Serial port permissions.** Add yourself to `dialout`, then log out and back in:
   ```bash
   sudo usermod -aG dialout $USER
   groups          # should list dialout
   ```
2. **Kernel headers**, needed to build the driver:
   ```bash
   sudo apt install linux-headers-$(uname -r)
   ```
3. **Build and install the Exar driver.** Run this as root, from the driver directory:
   ```bash
   cd Linux_Software/Teknic_SC4Hub_USB_Driver/ExarKernelDriver
   sudo ./Install_DRV_SCRIPT.sh
   ```
   The script builds `xr_usb_serial_common.ko` and installs it under `/lib/modules`. It
   adds the module to `/etc/modules` so it loads at boot, then moves the connected hub
   from `cdc_acm` to the Exar driver. Full instructions are in
   [`driver_readme.txt`](Linux_Software/Teknic_SC4Hub_USB_Driver/ExarKernelDriver/driver_readme.txt).
   The driver is built for one kernel version, so rerun the script after a kernel upgrade.
4. **Install the udev rule.** Teknic's script moves the hub only once. Whenever the hub
   reconnects (replugging, power changes, rebooting), `cdc_acm` claims it again.
   [`src/99-teknic-sc4hub.rules`](src/99-teknic-sc4hub.rules) moves it back to the Exar
   driver each time:
   ```bash
   sudo cp src/99-teknic-sc4hub.rules /etc/udev/rules.d/
   sudo udevadm control --reload
   ```
5. **Check the binding.** The hub should appear as `/dev/ttyXRUSB0`:
   ```bash
   ls -l /dev/ttyXRUSB0
   for i in /sys/bus/usb/devices/*:1.0; do
     grep -q 2890 "$i/../idVendor" 2>/dev/null && echo "$i -> $(basename "$(readlink -f "$i/driver")")"
   done   # expect: cdc_xr_usb_serial
   ```

To move a hub that's on `cdc_acm` by hand (its USB address, here `1-1:1.0`, depends on
the port):
```bash
sudo sh -c 'echo -n 1-1:1.0 > /sys/bus/usb/drivers/cdc_acm/unbind; echo -n 1-1:1.0 > /sys/bus/usb/drivers/cdc_xr_usb_serial/bind'
```

N.B. There was a typo in the driver source code that required editing before it will compile.

Edit 'xr_usb_serial_common.c' around line 1798:
```bash
sed -i 's/wCountyCode0/wCountryCode0/g' xr_usb_serial_common.c
```

## cpview: Linux Version of the Teknic ClearView Windows Application

'cpview' is a Qt 6 desktop app that covers the core ClearView workflow for
ClearPath-SC motors (SCSK/SCHP) on an SC4-Hub. It doesn't support ClearPath-MC/SD
motors; for those, use ClearPath MSP on Windows. The source is in [`src/`](src/), and
[`src/README.md`](src/README.md) also describes it.

### Features

- **Toolbar:** Scan for hubs, Connect/Disconnect, Enable/Disable, and **E-STOP**
  (`Esc`). E-STOP sends an abrupt e-stop to every node; press *Clear Alerts* to resume.
  Enable also clears alerts and node stops before enabling. **Plot Window** moves the
  plot into its own window (`Esc` still E-stops there); closing that window or
  unchecking the button puts it back in its tab.
- **Node tree:** every port and node found, labeled with model, serial number, and user
  ID. Selecting a node shows it in the tabs.
- **Status tab:** model, type, serial number, firmware/hardware versions, resolution,
  user ID and description. It has status lights (Ready, Enabled, Move Done, In Motion,
  Alert, Warning, Motion Blocked, ±Limit, Inputs A/B, …) and live position, commanded
  position, tracking error, velocity, torque, RMS level, bus voltage, and temperature.
  It also shows the alert, warning, and status-register text and has a Clear Alerts
  button.
- **Motion tab:** units (RPM or counts/s; % of max or amps), press-and-hold jog,
  velocity moves, and position moves (relative/absolute, dwell, repeat, back-and-forth).
  It also has homing (when the motor's config defines it) and velocity, acceleration,
  jerk (RAS), and dwell limits.
- **Config tab:** load and save `.mtr` config files (ClearView-compatible), restore
  factory defaults, and edit the user ID and description. It also sets the global
  torque limit, software limits, and position tracking limit, and has a raw parameter
  table covering every `CPM_P_*` register. Double-click a value to write it, after a
  confirmation prompt. **Homing setup...** opens ClearView's Homing Setup dialog (see
  [Homing setup](#homing-setup)).
- **Plot tab:** plot of measured/commanded position, tracking error, velocity, and
  torque, with ClearView-style triggering (see [Plot triggers](#plot-triggers)), an
  adjustable time window, a dashed trigger marker at t = 0, Clear, and CSV export.
- **Log panel:** timestamped messages. Errors, and any new or changed motor alert
  (e.g. `Node alert: BusVoltageLow`), appear in red and in the status bar.

Not available, because sFoundation has no public API for them: auto-tuning, firmware
updates, and ClearView's high-speed scope. For those, use ClearView on Windows. The
live readouts and plot come from polling, at 20 updates per second.

### Plot triggers

The Plot tab's trigger controls follow ClearView's soft scope (ClearPath-SC User
Manual, "ClearView Soft Scope"), but triggers are detected in software from the 20 Hz
polled samples, not by the motor.

| Mode | Behavior |
|---|---|
| Auto | Rolling view of the last *Window* seconds; no triggering |
| Normal | Waits for the trigger, captures one window, shows it, and re-arms. The last capture stays on screen until the next one completes |
| Single | Like Normal, but switches to Stop after one capture |
| Force (button) | Triggers on the newest sample, captures one window, then switches to Stop. Works from any mode |
| Stop | Freezes the display (the rolling view or the last capture). Data is still recorded for CSV export |

| Trigger on | Fires when |
|---|---|
| Start of positive/negative/any command | Commanded motion begins (commanded velocity becomes nonzero or commanded position starts changing) |
| End of positive/negative/any command | Commanded motion ends |
| End of positive/negative/any settled move | Move Done turns on; the direction is that of the last commanded motion |
| Alert (shutdown or exception) | An alert appears |
| Rising/falling slope | The selected trace crosses *Level* upward/downward; the crossing time is interpolated between samples |

*Position* puts the trigger point at 10%, 50%, or 90% of the window; captured plots
show time relative to the trigger. The status label shows Rolling, Armed, Triggered,
or Stopped. Changing any trigger setting re-arms.

Limitations: events are resolved only to the 50 ms poll period, and a move shorter
than one poll can be missed entirely. ClearView's "Voltage/Torque/Speed Limit" source
isn't available, since sFoundation doesn't report it. CSV export always writes the
whole recorded history, not just the current capture.

### Homing setup

The Config tab's **Homing setup...** button opens a copy of ClearView's Homing Setup
dialog (ClearPath-SC User Manual, "Homing Setup"). It reads the node's settings when it
opens and writes them when you press OK. Use the Motion tab's **Start Homing** to run
the homing sequence.

| Dialog setting | Stored in | Notes |
|---|---|---|
| Homing direction CCW (+) / CW (−) | `CPM_P_APP_CONFIG_REG` bit 19 (`HomingDirection`) | Set means CW |
| Max velocity (RPM) | `CPM_P_APP_HOMING_VEL` (512) | Stored as a positive value in counts/s; RPM = counts/s × 60 / counts per rev |
| Max accel (RPM/s) | `CPM_P_APP_HOMING_ACCEL` (514) | Stored in counts/s²; same scaling |
| Target: change of input / hard stop | `CPM_P_HW_CONFIG_REG` bit 14 (`HomingStyle`) | Set means hard stop |
| Home input (A or B, turns on or off) | `CPM_P_XPS_FEAT_AT_HOME` (563) | See the table below. Set to 0 for a hard stop |
| Homing torque limit (% of max) | `CPM_P_DRV_HARDSTOP_TRQ_LIM` (363) | Stored in amps; % = amps × 100 / `CPM_P_DRV_I_MAX` (the same scale as the torque unit "% of max") |
| Offset distance (cnts) | `CPM_P_APP_HOMING_OFFSET` (513) | Always positive. The label beside it shows the resulting direction |
| Offset direction reverse of / same as home | `CPM_P_APP_CONFIG_REG` bit 16 (`OffsetMoveInHomeDir`) | Set means same as home |

sFoundation doesn't document the encoding of `CPM_P_XPS_FEAT_AT_HOME`. These values
come from comparing `.mtr` files that ClearView saved with each input option
(`XPSFeatAtHome` in the file). Each value is the matching status-register input bit
(`InA`, `InB`, `InvInA`, `InvInB`; bits 22–25) shifted down by 16. `XPSInvertIn` stays 0
for every option.

| Home input | `CPM_P_XPS_FEAT_AT_HOME` |
|---|---|
| A input turns on | `0x0040` |
| B input turns on | `0x0080` |
| A input turns off | `0x0100` |
| B input turns off | `0x0200` |

Notes:
- If the node's input value is not one of these four, no input is selected, and the
  dialog asks you to choose one.
- If the node uses an extended homing mode (`HomingExtended` in the hardware config,
  e.g. home to shaft position), the dialog warns that it doesn't show it, and leaves the
  mode as it is.
- ClearView's Homing Actions window (simulated homing, "make current position home")
  is not reproduced.
- Untested: whether ClearView also clears the home input when you choose Hard Stop.
  cpview clears it, so a leftover input can't end a hard-stop home early.
- Untested: whether the node accepts these writes while it's enabled. If it refuses,
  the error appears in the log; disable the motor and try again.

### Build

Prerequisites: the USB driver setup above, and the sFoundation library built once (see
[Building the library](#building-the-library)). Then:

```bash
sudo apt install cmake qt6-base-dev qt6-charts-dev
cmake -S src -B build
cmake --build build -j
./build/cpview
```

- Qt 6.10 needs **CMake 3.22 or newer**. On this machine a stale CMake 3.18 in
  `/usr/local/bin` shadows the system's CMake 4.2, so use `/usr/bin/cmake` (or remove the
  old copy).
- The executable links to `libsFoundation20.so` in
  `Linux_Software/sFoundation/sFoundation/` through an rpath, and the library loads
  `MNuserDriver20.xml` from that same directory. CMake creates the
  `libsFoundation20.so.1` link there if it's missing. To use another copy, configure
  with `-DSFND_ROOT=/path/to/sFoundation`.
- The build writes `build/compile_commands.json` for clangd and other editor tooling.

### Using it

1. Power the hub (24 V logic) and the motor (bus supply), and connect USB.
2. Close ClearView, the SDK examples, and any other program using the hub, because only
   one program can open the port.
3. Start `./build/cpview`. It scans for hubs at startup; press **Connect**, and the first
   node is selected.
4. Press **Enable**. The motor runs a short sensorless startup sweep (the shaft turns a
   little) before it reports Ready.
5. Use the Motion tab. Keep the first moves slow until the motor is tuned for its load.

### Code layout

| File | Role |
|---|---|
| `main.cpp` | Starts the application and registers the custom types passed between threads |
| `MotorWorker.*` | Makes every sFoundation call on a single worker thread. It polls the selected node every 50 ms (a poll takes ~25 ms on the wire) and emits `Snapshot`s; errors become `error()` signals |
| `MainWindow.*` | Toolbar, node tree, tabs, and log; owns the worker thread |
| `tabs/InfoStatusTab.*`, `MotionTab.*`, `ConfigTab.*`, `PlotTab.*` | The four tabs. They reach the worker through `onWorker()`, which makes queued calls |
| `tabs/HomingDialog.*` | Homing Setup dialog, opened from the Config tab. The worker reads and writes it with `readHoming()`/`writeHoming()` |
| `CpmParams.inc` | ID, name, and description of every parameter, generated from the `_cpmParams` enum in `inc/inc-pub/pubCpmRegs.h` |
| `99-teknic-sc4hub.rules` | udev rule that moves the hub from `cdc_acm` to the Exar driver (see [USB Driver Setup](#usb-driver-setup)) |
| `CMakeLists.txt` | Build file. Finds Qt 6 Widgets and Charts and sFoundation, and sets the rpath |

## sFoundation: Teknic-provided Library for Controlling CPM-SCSK Motors

sFoundation is Teknic's C++ class library for ClearPath-SC motors. It finds hubs and
opens ports, enumerates nodes, and handles enabling and disabling, moves, homing,
status, alerts, parameters, and config files. It lives in
[`Linux_Software/sFoundation/`](Linux_Software/sFoundation/) (extracted from
`sFoundation.tar`). Teknic's setup notes are in
[`Linux_Software/readme.txt`](Linux_Software/readme.txt), and the license is
[`Linux_Software/Teknic sFoundation License.pdf`](Linux_Software/Teknic%20sFoundation%20License.pdf).

| Path | Contents |
|---|---|
| `sFoundation/` | Library sources (`src/`, `src-linux/`), `Makefile`, the built `libsFoundation20.so`, and `MNuserDriver20.xml` |
| `inc/inc-pub/` | Public headers. Start with `pubSysCls.h` (the `SysManager`, `IPort`, and `INode` classes); `pubCpmRegs.h` defines the status, alert, and parameter registers |
| `inc/inc-private/`, `LibINI/`, `LibLinuxOS/`, `LibXML/` | Internal headers and support libraries used to build the library |
| `SDK_Examples/` | Teknic's standard examples |
| `Beta_SDK_Examples/` | Teknic's newer, not fully tested examples |

### Building the library

```bash
cd Linux_Software/sFoundation/sFoundation
make                                   # builds libsFoundation20.so here
ldconfig -n .                          # creates the libsFoundation20.so.1 soname link
```

You can leave the library where it was built; cpview and the examples find it through
an rpath. For a system-wide install, copy `libsFoundation20.so` and
`MNuserDriver20.xml` to `/usr/local/lib` and run `sudo ldconfig`. Details are in
section 3 of [`Linux_Software/readme.txt`](Linux_Software/readme.txt).

### Using it in your own program

Add `-I Linux_Software/sFoundation/inc/inc-pub`, and link with `-lsFoundation20 -lpthread`.
If the library isn't in a system directory, add `-L<dir> -Wl,-rpath=<dir>`. A minimal
read-only session:

```cpp
#include "pubSysCls.h"
using namespace sFnd;

SysManager *mgr = SysManager::Instance();
std::vector<std::string> hubs;
SysManager::FindComHubPorts(hubs);           // e.g. "/dev/ttyXRUSB0"
mgr->ComHubPort(0, hubs[0].c_str());
mgr->PortsOpen(1);                           // throws mnErr on failure
INode &node = mgr->Ports(0).Nodes(0);
printf("%s, SN %u\n", node.Info.Model.Value(), node.Info.SerialNumber.Value());
mgr->PortsClose();                           // call this even after a failed PortsOpen
```

Every call can throw `mnErr`. Call `PortsClose()` even when `PortsOpen()` fails:
otherwise the library's background threads keep running and crash during program exit.

### Beta SDK Examples

[`Linux_Software/sFoundation/Beta_SDK_Examples/`](Linux_Software/sFoundation/Beta_SDK_Examples/)
holds Teknic's newer examples. Most open only the first hub found. Teknic says they
build and run on Linux but haven't been through its full test plan. Build one with
`make RPATH=<path to libsFoundation20.so>`, e.g.
`make RPATH=$PWD/../../sFoundation`.

| Example | What it shows | Moves the motor? |
|---|---|---|
| `SCNetworkReport` | Lists every port and node on the network | No |
| `NodeAlerts-Status` | Reads the status register, checks for alerts, clears alerts and node stops | No |
| `SC4-IO` | Drives the hub's two brake/general-purpose outputs; reads the Global Stop input and each node's inputs A/B | No (switches outputs) |
| `LoadingConfigFile` | Loads a `.mtr` config file onto a node | No (rewrites the config) |
| `PositionMoves` | Sequential position moves on one node, in both directions | Yes |
| `MotionVelocity` | Sequential velocity moves on one node, in both directions | Yes |
| `MotionDualAxis` | Synchronized position moves on two or more nodes | Yes |
| `Homing` | Starts homing and checks whether the node was homed | Yes |
| `SingleThreaded(Polling)` | A full single-threaded program with an `Axis` state-machine class | Yes |
| `MultiThreaded(Attentions)` | Event-driven multi-threaded program using attentions and triggered moves (Advanced firmware only) | Yes |
| `ProjectTemplate` | A starting point for new programs | No |

### SDK_Examples

[`Linux_Software/sFoundation/SDK_Examples/`](Linux_Software/sFoundation/SDK_Examples/)
holds Teknic's standard examples. Each one's `README.TXT` describes it. They open up to
three hubs, and they build the same way (`make RPATH=...`).

| Example | What it shows | Moves the motor? |
|---|---|---|
| `HelloWorld` | Opens the ports and lists the nodes | No |
| `Example-StatusAlerts` | Checks every node for alerts and clears them | No |
| `Example-GPIO` | Briefly sets the hub's brake output 0 and reads each node's inputs A/B | No (switches an output) |
| `Example-Homing` | Homes each node whose config defines homing | Yes |
| `Example-Motion` | Sequential moves, one node at a time, with homing first if configured | Yes, repeatedly in one direction |
| `Example-SingleThreaded` | Polling `Axis` class that exercises basic features | Yes, repeatedly in one direction |
| `Example-MultiThreaded` | `Axis` and `Supervisor` threads with triggered, synchronized moves (Advanced firmware only) | Yes |

Teknic says the motion examples assume either an unloaded motor with factory settings
or one auto-tuned on its mechanics. Don't run the ones that move "repeatedly in one
direction" on an axis with limited travel.

## Documentation

| File | Contents |
|---|---|
| [`docs/Clearpath-SC User Manual.pdf`](docs/Clearpath-SC%20User%20Manual.pdf) | **ClearPath-SC user manual, v1.45 (Feb 2026)**: wiring, power supplies, SC4-Hub, ClearView, specifications, part-number key. Use this one. |
| [`docs/clearpath_user_manual.pdf`](docs/clearpath_user_manual.pdf) | Manual for the *other* ClearPath families (MCVC, MCPV, SDSK, SDHP, v3.27). It doesn't apply to SC motors |
| [`docs/S-FoundationRef.chm`](docs/S-FoundationRef.chm) | sFoundation API reference: classes, register fields, and what each alert means, with remedies. Open it with `kchmviewer` or `xchm`. It's identical to `Linux_Software/doc/S-FoundationRef.chm` |
| [`docs/Readme.txt`](docs/Readme.txt) | Teknic's note about the API reference |
| [`Linux_Software/readme.txt`](Linux_Software/readme.txt) | Teknic's Linux setup guide: permissions, building and installing sFoundation, the USB driver, the examples, and building your own program |
| [`Linux_Software/doc/readme.txt`](Linux_Software/doc/readme.txt) | Which CHM viewers to use |

### Windows Software

The installers in [`bin/`](bin/) only run on Windows. They're kept here for what cpview
can't do, such as auto-tuning and firmware updates.

| File | Purpose |
|---|---|
| `ClearView_Install_1.7.139.exe` | ClearView 1.7.139, Teknic's setup and tuning program for ClearPath-SC motors. It includes the Windows sFoundation SDK and firmware |
| `ClearPath_MSP_2.0.127.exe` | ClearPath MSP 2.0.127, the setup program for ClearPath-MC/SD motors (not SC) |
| `ClearPath Port Driver Install.exe` | Windows USB driver for the SC4-Hub |

### CAD Files

| File | Contents |
|---|---|
| [`cad/23xx CPM Dimensional Drawing.pdf`](cad/23xx%20CPM%20Dimensional%20Drawing.pdf) | Dimensional drawing for NEMA 23 ClearPath motors, with the "L" body length for each model |
| `cad/CPM-231x_-LS.STEP` | STEP (AP203) model of a NEMA 23, 82 mm (231x) motor with the standard 3/8" shaft |
| `cad/CPM-231x_-Q.STEP` | The same motor with the 1/4" shaft option |
| `cad/SC4-HUB .STEP` | STEP model of the SC4-Hub |

## Troubleshooting

Problems hit while bringing up the motor, and what fixed them:

| Symptom | Cause | Fix |
|---|---|---|
| No `/dev/ttyXRUSB0`; the hub appears as `/dev/ttyACM0`, or "No SC hubs found" | `cdc_acm` claimed the hub after it reconnected | Install the udev rule, or move the hub by hand ([USB Driver Setup](#usb-driver-setup)) |
| Connect fails: "Serial Port … Initialization problem" (`0x80040601`) | The hub was found but no motor answered | Power the motor, and check its cable and the hub jumpers |
| A program crashes at exit with `Assertion 'rxTraces' failed` | `PortsClose()` wasn't called after `PortsOpen()` failed | Always call `PortsClose()` (cpview does) |
| Enable fails with `BusVoltageLow` | The bus was at 23.4 V. The absolute minimum is 21.5 V, but at 23.4 V the motor still wouldn't enable, while it enabled fine at 33.6 V | Use a 24–75 V supply, measured at the motor's connector; 36–48 V leaves headroom |
| Enable fails with `NoCommFailed` | The sensorless startup sweep ("no commutation") couldn't turn the shaft: binding, external load, a weak supply, or the wrong config | Make sure the shaft turns freely, and check that the supply doesn't sag during enable |
| `EStopped` alert at power-up; Motion Blocked | A node stop is latched | Press Clear Alerts (Enable also clears it) |
| CMake: "CMake 3.22 or higher is required to use Qt" | An old `cmake` comes first in `PATH` | Use `/usr/bin/cmake` |

## License

The cpview code is under the MIT license ([`LICENSE`](LICENSE)). Teknic's software and
documents (sFoundation, the USB driver, the manuals, the CAD files, and the installers)
are under Teknic's own terms; see
[`Linux_Software/Teknic sFoundation License.pdf`](Linux_Software/Teknic%20sFoundation%20License.pdf).
