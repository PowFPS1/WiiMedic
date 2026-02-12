<img width="128" height="48" alt="image" src="https://github.com/user-attachments/assets/6937a77d-bce6-4666-9780-0feb9afc5b78" />


# WiiMedic - Wii System Diagnostic & Health Monitor

**Version 1.0.0** | Wii Homebrew Application

A comprehensive all-in-one diagnostic tool for the Nintendo Wii. As Wii consoles age (now 20 years old!), hardware issues become increasingly common. WiiMedic gives you a complete picture of your system's health and generates shareable reports for community troubleshooting.

---

## Features

### 1. System Information
- Console region, firmware version, and video standard
- Hollywood/Broadway hardware revisions
- Device ID and Boot2 version (with BootMii compatibility warning)
- Memory arena status (MEM1/MEM2)
- Display settings (aspect ratio, progressive scan)

### 2. NAND Health Check
- Scans NAND filesystem cluster and inode usage
- Visual usage bar graphs with color-coded warnings
- Directory-level scan (/sys, /ticket, /title, /shared1, /tmp, /import)
- Detects interrupted title installations
- Calculates a health score out of 100
- Provides actionable recommendations

### 3. IOS Installation Scan
- Enumerates ALL installed IOS versions with revision numbers
- Detects stub IOS (potential problem sources)
- Identifies cIOS installations (d2x, Waninkoko, etc.)
- Detects BootMii IOS
- Descriptions for important IOS slots
- Warns if no cIOS is found for USB loader compatibility

### 4. Storage Speed Test
- Benchmarks SD card read/write speeds (1MB test, 3 iterations)
- Benchmarks USB drive read/write speeds
- Reports speed ratings (Excellent/Acceptable/Slow)
- Counts homebrew apps in /apps directory
- Tips for optimal storage configuration

### 5. Controller Diagnostics
- Tests all 4 GameCube controller ports
- Tests all 4 Wii Remote channels
- Real-time button, stick, and trigger readings
- Detects analog stick drift with threshold warnings
- Identifies Wii Remote extensions (Nunchuk, Classic Controller, etc.)
- Battery level monitoring for Wii Remotes
- IR sensor functionality check

### 6. Network Connectivity Test
- WiFi module initialization test
- IP address configuration display
- DHCP validation
- TCP connectivity tests to known servers
- Internet connectivity rating (Full/Partial/None)
- Tips for Wiimmfi and WiiLink connectivity

### 7. Full Report Generator
- Runs all diagnostics and saves to `sd:/WiiMedic_Report.txt`
- Shareable plain text format
- Perfect for pasting into forum posts or Reddit when asking for help

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
- libogc (included with devkitPro)
- libfat (included with devkitPro)

### Build Steps
```bash
# Install devkitPro (if not already installed)
# Follow instructions at https://devkitpro.org/wiki/Getting_Started

# Set environment variable
export DEVKITPPC=/opt/devkitpro/devkitPPC

# Clone and build
cd WiiMedic
make

# Output: boot.dol (copy to SD:/apps/WiiMedic/)
```

### Build on Windows
```powershell
# After installing devkitPro via installer
# Open MSys2 terminal from devkitPro
cd WiiMedic
make
```

### Clean Build
```bash
make clean
make
```

---

## Controls

| Button | Action |
|--------|--------|
| **D-Pad Up/Down** | Navigate menu |
| **A Button** | Select / Confirm |
| **B Button** | Return to menu (from sub-screen) |
| **HOME** (Wii Remote) / **START** (GC Controller) | Exit to Homebrew Channel |

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

- **Language:** C
- **Toolchain:** devkitPPC (GCC for PowerPC)
- **Libraries:** libogc, libfat, wiiuse, bte
- **Target:** Nintendo Wii (Homebrew Channel)
- **Output:** DOL executable
- **Architecture:** PowerPC 750CL (Broadway)
- **Compatibility:** All Wii models (RVL-001, RVL-101), Wii U vWii

---

## Why WiiMedic?

As of 2026, the Nintendo Wii is 20 years old. The homebrew community is thriving, but aging hardware brings challenges:

- **NAND degradation** - Bad blocks develop over time
- **Stick drift** - Analog sticks wear out on controllers
- **WiFi issues** - Wireless modules can degrade
- **Storage problems** - SD cards and USB drives fail silently
- **Softmod confusion** - Users can't easily verify their setup is correct

People frequently post on r/WiiHacks and GBAtemp asking "is my Wii broken?" or "why isn't this working?" WiiMedic gives them (and you) a quick, comprehensive answer.

---

## License

This project is licensed under the GNU General Public License v2 (GPLv2).

---

## Credits

- Built with [devkitPro](https://devkitpro.org/) / [libogc](https://github.com/devkitPro/libogc)
- Inspired by the Wii homebrew community's need for better diagnostic tools
- Thanks to the r/WiiHacks and GBAtemp communities

---

*Stay healthy!* 🎮
