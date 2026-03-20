<div align="center">
  <img width="128" height="48" alt="WiiMedic logo" src="https://github.com/user-attachments/assets/39cc7069-1e13-48b3-b09f-b41841d0db1d" />
</div>

# WiiMedic
### Wii System Diagnostic & Health Monitor — v1.2.0

---

Wii consoles are nearly 20 years old now. Things can go wrong, however sometimes it's obvious (won't boot, bad disc read), but a lot of the time you just get weird behavior and dont know where to start. That's what WiiMedic is for. it gives you a full picture of what's going on inside your system, so you can diagnose it yourself or share a report with someone who can help.

I built this because I kept seeing people post vague "my Wii is broken" threads with zero useful info. Now you can just run WiiMedic, generate a report, and paste it. Done.

---

## What it does

**System Info** — Firmware version, hardware revision, whether you have BootMii or Priiloader set up, and an overall "brick protection" rating so you know how safe you are.

**NAND Check** — Scans your NAND filesystem for leftover stuff, space issues, and interrupted title installs that can cause issues down the line.

**IOS Scan** — Shows every IOS installed on your system, including cIOS and stub configurations. Useful for diagnosing compatibility issues with specific games or homebrew.

**Storage Speed Test** — Benchmarks your SD card or USB drive and tells you whether it's fast enough for the homebrew you're trying to run. Some things are picky.

**Controller Diagnostics** — Live input monitor for GameCube controllers and Wii Remotes. Checks battery level, stick drift, and IR sensor function. Good for figuring out if a controller is broken or miscalibrated.

**Network Test** — Scans for nearby access points and pulls your WiFi module info (MAC address, firmware version, channel info). The network stuff is threaded now, so it won't just hang and make you think the app crashed.

**Report Generator** — Saves everything to `WiiMedic_Report.txt` on your SD card.

---

## Installation

Just drop it in your `apps` folder like any other homebrew.

**SD card:** Copy the `WiiMedic` folder to `SD:/apps/`  
**USB drive:** Copy it to `USB:/apps/` — use the bottom port on the back of the Wii

The folder should look like this:
```
/apps/WiiMedic/
├── boot.dol
├── meta.xml
└── icon.png
```

Launch it from the Homebrew Channel.

---

## Controls

| Button | Action |
|--------|--------|
| D-Pad Up/Down | Navigate |
| A | Select |
| B | Back to menu |
| HOME / START (GC) | Exit to Wii System Menu |

Works with Wii Remote or GameCube controller.

---

## Sharing a report

Run the diagnostics, go to Report Generator, save. Then:

1. Pull the SD card out, stick it in your PC
2. Open `WiiMedic_Report.txt`
3. Paste it wherever — Reddit, GBAtemp, Discord, wherever you're getting help

The report is designed to be readable by people who aren't experts, but detailed enough that people who are can use it.

---

## Building from source

You'll need devkitPro with devkitPPC installed, plus libogc 3.0.0+ and libfat.

```bash
export DEVKITPPC=/opt/devkitpro/devkitPPC
make
```

To build a release zip:
```bash
make dist
# outputs WiiMedic_v1.2.0.zip, ready to attach to a GitHub Release
```

---

## What's new in v1.2.0

- **Priiloader version detection** — it now reads the exact version string from the NAND binary instead of just telling you "Priiloader: yes/no"
- **Threaded network init** — network tests run on a separate thread now. The app stays responsive while it does its thing instead of freezing for 10 seconds
- **Battery bars actually match the Wii Menu** — the old version showed raw hex values which was pretty uselss. Now it shows 0–4 bars, same as what you'd see in the system menu (not perfect)
- **Drift detection improvements** — better thresholds, more reliable across different GameCube controller and Wii extension configurations
- **Report generation fixes** — there was a bug where reports would get cut off if WiFi hardware info was missing or corrupt. That's fixed

Full changelog at the bottom of this file.

---

## Compatibility

Works on all Wii models (RVL-001 and RVL-101) and Wii U vWii.

**Toolchain:** devkitPPC (GCC for PowerPC)  
**Libraries:** libogc, libfat, wiiuse, bte

---

## Credits

Developed by **PowFPS1**

**Abdelali221** helped a lot with the WiFi card info and AP scanning.

Built with [devkitPro](https://devkitpro.org/) and [libogc](https://github.com/devkitPro/libogc). Thanks to r/WiiHacks and GBAtemp for feedback and testing.

---

## Full Changelog

### v1.2.0
- Priiloader version detection (reads version string from NAND binary directly)
- Threaded network initialization — UI stays smooth during tests
- Battery display chnaged to show 0–4 bars matching the Wii Menu (still not perfect)
- Fixed report truncation bug when WiFi hardware data is missing/corrupt
- Added `s_wdinfo_valid` flag for better handling of corrupt WiFi hardware info
- Improved stick drift detection across GameCube and Wii extension controllers
- `memcpy`-based header patching in report generation

### v1.1.0
- Brick protection check (Priiloader, BootMii/boot2, BootMii/IOS) with overall rating
- WiFi card info: MAC address, firmware version, country code, active channels
- WiFi AP scanner: nearby networks with SSID, signal strength, channel, security type
- Scrollable diagnostic screens (up/down line-by-line, left/right page)
- Fixed Wii Remote detection in Controller Diagnostics and Report
- Report now detects an existing file and asks whether to replace it, keep both, or cancel
- Report saves to USB if no SD card is present
- All module output goes through the scroll buffer now

### v1.0.0
- Initial release
