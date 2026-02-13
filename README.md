<img width="128" height="48" alt="image" src="https://github.com/user-attachments/assets/6937a77d-bce6-4666-9780-0feb9afc5b78" />


# WiiMedic

Diagnostic tool for the Wii. Checks your system health, tests hardware, and spits out a report you can share when something's not working right.

**v1.1.0** — [WiiBrew Page](https://wiibrew.org/wiki/WiiMedic) · [Open Shop Channel PR](https://github.com/OpenShopChannel/Apps/pull/141)

---

## What it does

WiiMedic has 7 modules you can run from a simple menu:

- **System Info** — Region, video mode, Hollywood revision, Boot2 version, memory, running IOS, etc.
- **NAND Health** — Cluster/inode usage with visual bars, scans directories for issues, gives you a health score out of 100
- **IOS Scan** — Lists every IOS installed with revision numbers. Flags stubs, cIOS (d2x/Waninkoko), and BootMii
- **Storage Test** — Benchmarks your SD and USB read/write speeds. Tells you if your card is too slow for game loading
- **Controller Test** — Checks all 4 GC ports and Wii Remote slots. Reads sticks, buttons, triggers. Catches stick drift
- **Network Test** — Tests WiFi init, grabs your IP, tries connecting to Google DNS and Cloudflare to check internet
- **Full Report** — Runs everything above and dumps it to a text file on your SD/USB. Great for posting on Reddit or GBAtemp when you need help

All screens scroll if the output is longer than what fits on screen.

---

## Install

Download the [latest release](https://github.com/PowFPS1/WiiMedic/releases), extract the zip, and copy the `apps` folder to the root of your SD card or USB drive. You should end up with:

```
SD:/apps/WiiMedic/boot.dol
SD:/apps/WiiMedic/meta.xml
SD:/apps/WiiMedic/icon.png
```

Then just launch it from the Homebrew Channel. USB goes in the bottom port.

---

## Controls

| Button | What it does |
|--------|-------------|
| D-Pad Up/Down | Navigate menu, scroll through results |
| D-Pad Left/Right | Page up/down in scroll view |
| A | Select / Confirm |
| B | Back to menu |
| HOME / START | Quit to HBC |

Works with Wii Remote and GameCube controllers.

---

## Report

The report generator runs all 6 tests and writes everything to `WiiMedic_Report.txt` on your SD card (or USB if there's no SD). If there's already a report file, it'll ask you whether to overwrite it, save a new one alongside it, or just cancel.

No personal info is included besides the console's Device ID.

---

## Building

You need [devkitPro](https://devkitpro.org/) with devkitPPC installed.

```bash
cd WiiMedic
make
```

That gives you `boot.dol`. Copy it to `SD:/apps/WiiMedic/`. Use `make clean` first if you want a fresh build.

On Windows, run it through devkitPro's MSYS2 terminal.

---

## Why

Wiis are old. Stuff breaks — NAND goes bad, controllers drift, WiFi dies, SD cards get slow, and people end up confused about their IOS setup. You see it all the time on r/WiiHacks: "is my Wii broken?" or "why won't USB Loader work?"

This just gives you a quick way to check everything and get a report you can share.

---

## Changelog

**v1.1.0**
- Scrolling on all diagnostic screens
- Fixed Wii Remote not showing up in controller test and reports
- Report asks what to do if a previous report already exists
- Falls back to USB if SD isn't available for saving reports

**v1.0.0**
- First release. All 7 modules working

---

## Credits

Built with [devkitPro](https://devkitpro.org/) and [libogc](https://github.com/devkitPro/libogc). Thanks to r/WiiHacks and GBAtemp.
