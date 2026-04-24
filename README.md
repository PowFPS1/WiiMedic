<div align="center">
  <img width="128" height="48" alt="WiiMedic logo" src="https://github.com/user-attachments/assets/39cc7069-1e13-48b3-b09f-b41841d0db1d" />
</div>

# WiiMedic
### Wii System Diagnostic & Health Monitor — v1.3.0

---

Wii consoles are almost 20 years old now and go wrong, sometimes it's obvious like it won't boot or the disc drive sounds like it's dying, however, sometimes you get weird behavior and you don't really know where to start and I found that really frustrating. I kept seeing people post on forums with barely any information like "my Wii is broken" and nobody could help them because nobody knew what was going on inside the console. That's why I built this, I wanted something that would just tell you everything at once so you could either fix it yourself or at least give someone useful information when you're asking for help.

---

<img width="1477" height="812" alt="image" src="https://github.com/user-attachments/assets/ca5e2948-3398-40d0-ae9d-2b6278f13991" />
*Main menu running on Dolphin Emulator*

---

## what it does

**System Info** — shows you your firmware version, hardware revision, whether you have BootMii or Priiloader set up, and gives you an overall brick protection rating so you know how safe your console is if something goes wrong.

**NAND Check** — scans your NAND filesystem for leftover junk, space issues, and interrupted title installs that can cause problems down the line, gives you a health score out of 100 which I think is a nice touch.

**IOS Scan** — shows every IOS installed on your system including cIOS and stub configurations, this is really useful if you're having compatibility issues with specific games or homebrew and you don't know why.

**Storage Speed Test** — benchmarks your SD card or USB drive and tells you whether it's actually fast enough for the homebrew you're trying to run, some stuff is pretty picky about that.

**Controller Diagnostics** — live input monitor for GameCube controllers and Wii Remotes, checks battery level, stick drift, and whether the IR sensor is working. good for figuring out if a controller is broken or just needs to be recalibrated.

**Network Test** — scans for nearby access points and pulls your WiFi module info like your MAC address, firmware version, channel info. the network stuff runs on a separate thread now so it won't freeze up and make you think the app crashed, that used to be a problem.

**Report Generator** — saves everything to `WiiMedic_Report.txt` on your SD card so you can share it wherever you're asking for help.

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

run the diagnostics, go to Report Generator, save. then:

1. pull the SD card out and stick it in your PC
2. open `WiiMedic_Report.txt`
3. paste it wherever you're getting help

I tried to make the report readable for people who aren't super into Wii stuff, but detailed enough that people who are are able to use it to help you. I think its fairly well balanced but I'm not sure, it's better than nothing or just saying "my Wii is broken" with no other context.

---

## building from source

you'll need devkitPro with devkitPPC installed, plus libogc 3.0.0+ and libfat, I won't pretend the setup is super easy but the devkitPro website walks you through it pretty well.

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

## what's new in v1.3.0

- **Loading indicator** — report generation now shows a spinning `|/-\` animation during the steps that used to look frozen (system info and controller scan), so it's obvious something is happening
- **Controller detection fixed** — a GC controller sitting at rest (no buttons held, sticks centered) no longer shows as "not detected"
- **HBC exit fallback** — if the Homebrew Channel isn't installed under the usual title ID, it tries the alternate one and falls back to the Wii System Menu instead of crashing silently
- **IOS stub detection improved** — rev 65280 (0xFF00) is now correctly flagged as a Nintendo stub placeholder
- **IOS236 label corrected** — it's a cIOS installer slot, not BootMii IOS
- **Storage benchmark hardened** — fread/fwrite return values are now checked and the benchmark aborts cleanly if storage fails mid-test
- **Network functions use snprintf** — was using bare sprintf which is unsafe
- **NAND caching** — report generator no longer re-runs the full NAND scan if you already ran it from the menu
- **Priiloader info cached** — system info collects brick protection data once and reuses it, eliminating redundant ISFS I/O cycles
- **8KB stack buffer made static** — avoids a large stack frame in the report generator

full changelog at the bottom if you want all the details.

---

## compatibility

works on all Wii models (RVL-001 and RVL-101) and Wii U vWii.

**Toolchain:** devkitPPC (GCC for PowerPC)  
**Libraries:** libogc, libfat, wiiuse, bte

---

## credits

developed by **PowFPS1**

**Abdelali221** helped a lot with the WiFi card info and AP scanning, I genuinely could not have figured that part out without him.

built with [devkitPro](https://devkitpro.org/) and [libogc](https://github.com/devkitPro/libogc). thanks to r/WiiHacks and GBAtemp for feedback and testing, I appreciate everyone who took the time to try it out and tell me what was broken.

---

## full changelog

### v1.3.0
- Loading spinner during report generation (system info step and controller scan step)
- GC controller detection rewritten to use PAD_ScanPads() bitmask — idle controllers no longer show as disconnected
- HBC exit now tries HAXX and JODI title IDs with Wii Menu fallback
- IOS stub detection catches rev 65280 (0xFF00) Nintendo stub
- IOS236 label corrected from "BootMii IOS" to "cIOS installer slot"
- fread/fwrite return values checked in storage benchmark
- sprintf → snprintf in network helper functions
- Report generator skips NAND re-scan if already cached
- Priiloader/BootMii detection cached at module level (eliminates redundant ISFS I/O)
- 8KB section buffer in report generator changed to static

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
