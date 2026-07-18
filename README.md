<div align="center">
  <img width="128" height="48" alt="WiiMedic logo" src="https://github.com/user-attachments/assets/39cc7069-1e13-48b3-b09f-b41841d0db1d" />
</div>

<div align="center">

![Release](https://img.shields.io/github/v/release/PowFPS1/WiiMedic)
![Platform](https://img.shields.io/badge/platform-Wii%20%7C%20vWii-8a2be2)
![Downloads](https://img.shields.io/github/downloads/PowFPS1/WiiMedic/total)

</div>

# WiiMedic
### Wii System Diagnostic & Health Monitor — v1.3.1

---

I built this because I got tired of not knowing what was going on with my Wii. You get some weird behavior, you go to a forum, and everyone asks the same questions, what firmware are you on, do you have BootMii, what does your NAND look like, and you just don't know because nothing told you. I wanted something that would just lay it all out at once so you could either fix it yourself or at least walk into a conversation with actual useful information instead of just "my Wii is broken." 

Unfortunately I set the GitHub repo to private for a moment and lost all my stars in the process. If you find the project useful, a star on the repo would mean a lot!

---

<img width="1477" height="812" alt="image" src="https://github.com/user-attachments/assets/ca5e2948-3398-40d0-ae9d-2b6278f13991" />
*Main menu running on Dolphin Emulator*

---

## What's new in v1.3.1

Three things in v1.3.0 either weren't working right or were just wrong, so here's the fix release.

**Every module now shows a loading spinner.** Previously you'd select something like the NAND check or IOS scan and the screen would just sit there until it was done. Looked kind of like a crash. Now there's a live animation the whole time so you know it's running.

**The HBC exit was broken for almost everyone.** "Exit to Homebrew Channel" was only trying two very old title IDs from 2009. If you have a normal modern HBC install; which is basically everyone, it was falling back to the System Menu instead. Fixed to try the right IDs in the right order.

**The NAND health check was showing backwards data.** The function that reads NAND space usage returns *free* counts, not used counts. The code was treating them as used, so a nearly full NAND showed as nearly empty, the health score was wrong, and the bar graphs were mirrored. That's a big issue that is now fixed. :D

---

## Requirements

- Homebrew Channel installed
- SD card or USB drive formatted as FAT32
- Recommended: IOS58 (best NAND/ES access), but most IOS versions work fine

---

## What it does

**System Info** — Shows you your firmware version, hardware revision, whether you have BootMii or Priiloader set up, and gives you an overall brick protection rating so you know how safe your console actually is if something goes wrong.

**NAND Check** — Scans your NAND filesystem for leftover junk, space issues, and interrupted title installs that can cause problems down the line. gives you a health score out of 100 which I think is a nice touch honestly.

**IOS Scan** — Shows every IOS installed on your system including cIOS and stub configurations, this is really useful if you're having compatibility issues with specific games or homebrew and you don't know why, I ran into this a lot.

**Storage Speed Test** — Benchmarks your SD card or USB drive and tells you whether it's actually fast enough for the homebrew you're trying to run, some stuff is really picky about that and I didn't know that for a long time.

**Controller Diagnostics** — Live input monitor for GameCube controllers and Wii Remotes, checks battery level, stick drift, and whether the IR sensor is working. good for figuring out if a controller is actually broken or just needs to be recalibrated.

**Network Test** — Scans for nearby access points and pulls your WiFi module info like your MAC address, firmware version, channel info. the network stuff runs on a separate thread now so it won't freeze up and make you think the app crashed, that used to be a problem and it bothered me a lot.

**Report Generator** — Saves everything to `WiiMedic_Report.txt` on your SD card so you can share it wherever you're asking for help. I tried to make it readable for people who aren't super into Wii stuff but detailed enough that people who are can actually use it, either way it's better than just saying "my Wii is broken" with no other context.

---

## Installation

Just drop it in your apps folder like any other homebrew, nothing complicated about it.

**SD card:** copy the `WiiMedic` folder to `SD:/apps/`
**USB drive:** copy it to `USB:/apps/`

the folder should look like this:
```
/apps/WiiMedic/
├── boot.dol
├── meta.xml
└── icon.png
```

Launch it from the Homebrew Channel. I'd recommend running it under IOS58 if you can, it gives the best access to NAND and the ES functions, though it should work fine under most IOS versions.

---

## Controls

| Button | Action |
|--------|--------|
| D-Pad Up/Down | Navigate |
| A | Select |
| B | Back to menu |
| HOME / START (GC) | Exit to Wii System Menu |

Works with Wii Remote or GameCube controller, whichever you have plugged in.

---

## Sharing a report

Run the diagnostics, go to Report Generator, save. then just pull the SD card out, stick it in your PC, open `WiiMedic_Report.txt`, and paste it wherever you're getting help. that's really all there is to it.

---

## Building from source

You'll need devkitPro with devkitPPC installed, plus libogc 3.0.0+ and libfat. I won't pretend the setup is super easy but the devkitPro website walks you through it pretty well.

```bash
export DEVKITPPC=/opt/devkitpro/devkitPPC
make
```

to build a release zip:
```bash
make dist
# outputs WiiMedic_v1.3.1.zip, ready to attach to a GitHub Release
```

---

## Compatibility

Works on all Wii models (RVL-001 and RVL-101) and Wii U vWii.

**Toolchain:** devkitPPC (GCC for PowerPC)
**Libraries:** libogc, libfat, wiiuse, bte

---

## Credits

Developed by **PowFPS1**

**Abdelali221** helped with the WiFi card info and AP scanning.

built with [devkitPro](https://devkitpro.org/) and [libogc](https://github.com/devkitPro/libogc). thanks to r/WiiHacks and GBAtemp for the feedback and testing.

---

## Changelog

<details close>

### v1.3.1
- **Loading spinner everywhere now** — Every diagnostic screen shows a live spinning animation while it's working now, not just the report generator. Same idea as before, just wasn't applied everywhere it should've been.
- **HBC exit actually works now** — This one's kind of embarrassing, it was only trying the old `HAXX` (beta) and `JODI` (2010) title IDs, so if you were running a normal modern HBC install (`LULZ`, v1.0.8+) it would just put you into the System Menu instead of taking you back to HBC like it's meant to. Now it tries `LULZ` → `OHBC` (vWii) → `JODI` → `HAXX` → System Menu as a final resort.
- **NAND space was backwards** — Turns out `ISFS_GetUsage` gives you free space, not used, and I had it flipped. Used/free numbers, health score, bar graphs, all of them were showing the opposite, so a NAND that was nearly full looked nearly empty. that's a pretty bad one to have shipped, but it's fixed now.
- **Numbered report filenames fixed** — The alternate report path was writing to `sd://WiiMedic_Report_2.txt` with a double slash, which some FAT implementations just reject. fixed.
- **Brick rating mismatch fixed** — The rating in the saved report was using a stricter BootMii/boot2 check than what showed live on screen, so you could get GOOD on the display and PARTIAL in the report for no reason. both use the same logic now.
- **Cleaned up dead code** — `is_known_stub_revision()` Had a switch block that could never get hit. gone now.
- **Network module refactored** — The IP config and connection test logic was copy-pasted in two spots, which is how bugs like the ones above happen, so I pulled it into one shared helper.
- **Storage tips** — Added a note that SD cards over 32GB need to be FAT32, not exFAT, since that tends to trip people up.

### v1.3.0
- **Loading indicator** — Report generation now shows a spinning animation during the steps that used to look frozen, so it's obvious something is actually happening and the app isn't dead
- **Controller detection fixed** — A GC controller sitting at rest no longer shows as not detected, that was really annoying
- **HBC exit fallback** — If the Homebrew Channel isn't installed under the usual title ID it tries the alternate one and falls back to the Wii System Menu instead of crashing silently
- **IOS stub detection improved** — rev 65280 is now correctly flagged as a Nintendo stub placeholder
- **IOS236 label corrected** — It's a cIOS installer slot, not BootMii IOS, my bad on that one
- **Storage benchmark hardened** — The benchmark now aborts cleanly if storage fails mid-test
- **Network functions use snprintf** — Was using bare sprintf which is unsafe
- **NAND caching** — Report generator no longer re-runs the full NAND scan if you already ran it from the menu
- **Priiloader info cached** — Eliminates redundant ISFS I/O cycles
- **8KB stack buffer made static** — Avoids a large stack frame in the report generator

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
