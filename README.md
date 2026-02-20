<div align="center">
  <img width="128" height="48" alt="Image" src="https://github.com/user-attachments/assets/39cc7069-1e13-48b3-b09f-b41841d0db1d" />
</div>

# WiiMedic - Wii System Diagnostic & Health Monitor

**Version 1.2.0** | Wii Homebrew Application

A comprehensive all-in-one diagnostic tool for the Nintendo Wii. As Wii consoles age (now 20 years old!), hardware issues become increasingly common. WiiMedic gives you a complete picture of your system's health and generates shareable reports for community troubleshooting.

---

## Features

- **System Information & Health Data**: View firmware info, hardware revisions, BootMii/Priiloader status, and brick protection ratings.
- **NAND Health Check**: Scans filesystem, space usage, and detects interrupted title installations. 
- **IOS Installation Scan**: Enumerates all installed IOS versions, detects cIOS and stub IOS configurations.
- **Storage Speed Test**: Benchmark SD/USB speeds, rating them for optimal homebrew performance.
- **Controller Diagnostics**: Monitor GameCube & Wii Remote inputs, detects battery levels, stick drift, and IR sensor functionality.
- **Network Connectivity Test**: Includes a robust WiFi scanner that grabs AP information and WiFi hardware specs (MAC, firmware). Network tests are threaded to prevent hang-ups.
- **Report Generator**: Dumbs down all diagnostics into a saveable standard text file, ready for easy clipboard sharing.



---

## Changes since v1.1.0
- **Priiloader Version Detection**: Advanced scanner to detect the exact version of Priiloader installed on the NAND.
- **Threaded Network Initialization**: Network tests are now threaded, drastically improving responsiveness and preventing the app from hanging.
- **Refined Battery Display**: Accurately displays 0-4 battery bars to perfectly match the Wii Menu indicators, hiding unnecessary technical hex values.
- **Improved Controller Diagnostics**: Better stick drift detection and threshold warnings.
- **Safer Report Generation**: Hardened report generation system that prevents truncation issues when handling missing network hardware info.
- **Enhanced WiFi Hardware Validation**: Added specific detection to differentiate between WiFi module operational status versus missing/invalid hardware info.

---

## Installation

### Method 1: SD Card
1. Download the latest release
2. Copy the `WiiMedic` folder to `/apps/` on your SD card
3. The folder structure should be: `SD:/apps/WiiMedic/boot.dol`
4. Insert SD card into your Wii
5. Launch from the Homebrew Channel

### Method 2: USB Drive
1. Download the latest release
2. Copy the `WiiMedic` folder to `/apps/` on your USB drive
3. The folder structure should be: `USB:/apps/WiiMedic/boot.dol`
4. Insert USB drive into your Wii (use the bottom port)
5. Launch from the Homebrew Channel

### Required Files
```
/apps/WiiMedic/
├── boot.dol          # Main application
├── meta.xml          # App metadata for Homebrew Channel
└── icon.png          # App icon (optional, 128x48)
```

---

## Building from Source

### Prerequisites
- [devkitPro](https://devkitpro.org/) with devkitPPC
- libogc 3.0.0+ and libfat

### Build Steps
```bash
# Set environment variable if needed
export DEVKITPPC=/opt/devkitpro/devkitPPC

# Build the DOL file
make
```

### Create release zip (for GitHub Releases)
After building, create a zip with `boot.dol`, `meta.xml`, and `icon.png` (same layout users extract to SD/USB):

```bash
make dist
# Creates: WiiMedic_v1.1.0.zip (contains WiiMedic/boot.dol, meta.xml, icon.png)
```

Upload `WiiMedic_v1.1.0.zip` to a GitHub Release so users can download and copy the `WiiMedic` folder to `SD:/apps/` or `USB:/apps/`.

---

## Controls

| Button | Action |
|--------|--------|
| **D-Pad Up/Down** | Navigate menu |
| **A Button** | Select / Confirm |
| **B Button** | Return to menu (from sub-screen) |
| **HOME** (Wii Remote) / **START** (GC Controller) | Exit to Wii System Menu |

Works with both **Wii Remote** and **GameCube Controller**.

---

## Report Sharing

After generating a report, the file is saved as `WiiMedic_Report.txt` in the root of your SD card. To share it:

1. Remove SD card from Wii and insert into PC
2. Open `WiiMedic_Report.txt`
3. Copy and paste the contents into a forum post, Reddit thread, or Discord message
4. The report contains NO personal information beyond your Wii's Device ID

---

## Technical Details

- **Toolchain:** devkitPPC (GCC for PowerPC)
- **Libraries:** libogc, libfat, wiiuse, bte
- **Target:** Nintendo Wii (Homebrew Channel)
- **Compatibility:** All Wii models (RVL-001, RVL-101), Wii U vWii

---

## Changelog

### v1.2.0
- **Priiloader Detection System**: Detects installed Priiloader and extracts the exact version string from the NAND binary (e.g., "v0.10").
- **Threaded Network Init**: Network initialization is now placed on a separate thread, providing a much smoother UI experience during tests.
- **Accurate Battery Bars**: Completely revamped battery reporting to show precisely 0-4 bars corresponding to the real Wii Menu, stripping out irrelevant raw voltage data.
- **Safer Report Generation**: Fixed bugs that caused reports to be truncated if specific WiFi hardware data was invalid. Added `memcpy`-based header patching.
- **Improved Drift Detection**: Refined logic for detecting controller stick drift across GameCube controllers and Wii extensions.
- **WiFi Validation Flag**: Implemented a `s_wdinfo_valid` flag to gracefully handle scenarios where the driver works but hardware info is corrupt.
- **Simplified UI Presence Checks**: Priiloader presence checks simplified for standard diagnostics.

### v1.1.0
- **Brick Protection Check**: Detects Priiloader, BootMii (boot2), and BootMii (IOS) with a protection rating
- **Refined Exit Logic**: HOME button now returns to System Menu; Menu option exits to HBC
- WiFi Card Info: MAC address, firmware version, country code, enabled channels
- WiFi AP Scanner: scans nearby access points (SSID, signal, channel, security)
- Scrollable diagnostic screens (UP/DOWN line-by-line, LEFT/RIGHT page)
- Fixed Wii Remote detection in Controller Diagnostics and Report
- Report detects existing reports: replace, keep both, or cancel
- Report saves to USB if no SD card available
- All module output routed through scroll buffer

### v1.0.0
- Initial release with basic diagnostics and reporting functionality.

---

## Credits

- Developed by **PowFPS1**
- **Abdelali221** — help with WiFi card info and AP scan implementation in the network code
- Built with [devkitPro](https://devkitpro.org/) / [libogc](https://github.com/devkitPro/libogc)
- Inspired by the Wii homebrew community's need for better diagnostic tools

- Thanks to the r/WiiHacks and GBAtemp communities
