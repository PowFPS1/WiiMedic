<div align="center">
  <img width="128" height="48" alt="WiiMedic logo" src="https://github.com/user-attachments/assets/39cc7069-1e13-48b3-b09f-b41841d0db1d" />
</div>

<div align="center">

![Release](https://img.shields.io/github/v/release/PowFPS1/WiiMedic)
![License](https://img.shields.io/github/license/PowFPS1/WiiMedic)
![Platform](https://img.shields.io/badge/platform-Wii%20%7C%20vWii-8a2be2)
![Stars](https://img.shields.io/github/stars/PowFPS1/WiiMedic)

</div>

# WiiMedic
### Wii System Diagnostic & Health Monitor — v1.3.0

---

I built this because I got tired of not knowing what was going on with my Wii. You get some weird behavior, you go to a forum, and everyone asks the same questions, what firmware are you on, do you have BootMii, what does your NAND look like, and you just don't know because nothing told you. I wanted something that would just lay it all out at once so you could either fix it yourself or at least walk into a conversation with actual useful information instead of just "my Wii is broken."

---

<img width="1477" height="812" alt="image" src="https://github.com/user-attachments/assets/ca5e2948-3398-40d0-ae9d-2b6278f13991" />
*Main menu running on Dolphin Emulator*

---

## requirements

- Homebrew Channel installed
- SD card or USB drive formatted as FAT32
- Recommended: IOS58 (best NAND/ES access), but most IOS versions work fine

---

## what it does

**System Info** — shows you your firmware version, hardware revision, whether you have BootMii or Priiloader set up, and gives you an overall brick protection rating so you know how safe your console actually is if something goes wrong.

**NAND Check** — scans your NAND filesystem for leftover junk, space issues, and interrupted title installs that can cause problems down the line. gives you a health score out of 100 which I think is a nice touch honestly.

**IOS Scan** — shows every IOS installed on your system including cIOS and stub configurations, this is really useful if you're having compatibility issues with specific games or homebrew and you don't know why, I ran into this a lot.

**Storage Speed Test** — benchmarks your SD card or USB drive and tells you whether it's actually fast enough for the homebrew you're trying to run, some stuff is really picky about that and I didn't know that for a long time.

**Controller Diagnostics** — live input monitor for GameCube controllers and Wii Remotes, checks battery level, stick drift, and whether the IR sensor is working. good for figuring out if a controller is actually broken or just needs to be recalibrated.

**Network Test** — scans for nearby access points and pulls your WiFi module info like your MAC address, firmware version, channel info. the network stuff runs on a separate thread now so it won't freeze up and make you think the app crashed, that used to be a problem and it bothered me a lot.

**Report Generator** — saves everything to `WiiMedic_Report.txt` on your SD card so you can share it wherever you're asking for help. I tried to make it readable for people who aren't super into Wii stuff but detailed enough that people who are can actually use it, either way it's better than just saying "my Wii is broken" with no other context.

---

## installation

just drop it in your apps folder like any other homebrew, nothing complicated about it.

**SD card:** copy the `WiiMedic` folder to `SD:/apps/`
**USB drive:** copy it to `USB:/apps/`

the folder should look like this:
```
/apps/WiiMedic/
├── boot.dol
├── meta.xml
└── icon.png
```

launch it from the Homebrew Channel. I'd recommend running it under IOS58 if you can, it gives the best access to NAND and the ES functions, though it should work fine under most IOS versions.

---

## controls

| Button | Action |
|--------|--------|
| D-Pad Up/Down | Navigate |
| A | Select |
| B | Back to menu |
| HOME / START (GC) | Exit to Wii System Menu |

works with Wii Remote or GameCube controller, whichever you have plugged in.

---

## sharing a report

run the diagnostics, go to Report Generator, save. then just pull the SD card out, stick it in your PC, open `WiiMedic_Report.txt`, and paste it wherever you're getting help. that's really all there is to it.

---

## building from source

you'll need devkitPro with devkitPPC installed, plus libogc 3.0.0+ and libfat. I won't pretend the setup is super easy but the devkitPro website walks you through it pretty well.

```bash
export DEVKITPPC=/opt/devkitpro/devkitPPC
make
```

to build a release zip:
```bash
make dist
# outputs WiiMedic_v1.3.0.zip, ready to attach to a GitHub Release
```

---

## compatibility

works on all Wii models (RVL-001 and RVL-101) and Wii U vWii.

**Toolchain:** devkitPPC (GCC for PowerPC)
**Libraries:** libogc, libfat, wiiuse, bte

---

## credits

developed by **PowFPS1**

**Abdelali221** helped a lot with the WiFi card info and AP scanning.

built with [devkitPro](https://devkitpro.org/) and [libogc](https://github.com/devkitPro/libogc). thanks to r/WiiHacks and GBAtemp for the feedback and testing.

---

## changelog

### v1.3.0
- **Loading indicator** — report generation now shows a spinning animation during the steps that used to look frozen, so it's obvious something is actually happening and the app isn't dead
- **Controller detection fixed** — a GC controller sitting at rest no longer shows as not detected, that was really annoying
- **HBC exit fallback** — if the Homebrew Channel isn't installed under the usual title ID it tries the alternate one and falls back to the Wii System Menu instead of crashing silently
- **IOS stub detection improved** — rev 65280 is now correctly flagged as a Nintendo stub placeholder
- **IOS236 label corrected** — it's a cIOS installer slot, not BootMii IOS, my bad on that one
- **Storage benchmark hardened** — the benchmark now aborts cleanly if storage fails mid-test
- **Network functions use snprintf** — was using bare sprintf which is unsafe
- **NAND caching** — report generator no longer re-runs the full NAND scan if you already ran it from the menu
- **Priiloader info cached** — eliminates redundant ISFS I/O cycles
- **8KB stack buffer made static** — avoids a large stack frame in the report generator

### v1.2.0
- Priiloader version detection (reads version string from NAND binary directly)
- Threaded network initialization — UI stays responsive during tests
- Battery display changed to show 0–4 bars matching the Wii Menu (still not perfect)
- Fixed report truncation bug when WiFi hardware data is missing or corrupt
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
