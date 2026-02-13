/*
 * WiiMedic - system_info.c
 * Displays comprehensive system hardware and firmware information
 */

#include <gccore.h>
#include <malloc.h>
#include <ogc/isfs.h>
#include <ogc/machine/processor.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "system_info.h"
#include "ui_common.h"

/*---------------------------------------------------------------------------*/
/* Brick protection detection helpers                                        */
/*---------------------------------------------------------------------------*/
static bool detect_priiloader(void) {
  /* Priiloader leaves loader.ini in the System Menu data directory */
  s32 fd =
      ISFS_Open("/title/00000001/00000002/data/loader.ini", ISFS_OPEN_READ);
  if (fd >= 0) {
    ISFS_Close(fd);
    return true;
  }
  return false;
}

static bool detect_bootmii_ios(void) {
  /* BootMii IOS is typically installed as IOS254 or IOS236 */
  u32 tmd_size = 0;
  /* Check IOS254 first */
  u64 tid_254 = 0x00000001000000FEULL; /* IOS254 */
  if (ES_GetStoredTMDSize(tid_254, &tmd_size) >= 0 && tmd_size > 0)
    return true;
  /* Check IOS236 */
  u64 tid_236 = 0x00000001000000ECULL; /* IOS236 */
  tmd_size = 0;
  if (ES_GetStoredTMDSize(tid_236, &tmd_size) >= 0 && tmd_size > 0)
    return true;
  return false;
}

/*---------------------------------------------------------------------------*/
static const char *get_region_string(void) {
  switch (CONF_GetRegion()) {
  case CONF_REGION_JP:
    return "Japan (NTSC-J)";
  case CONF_REGION_US:
    return "Americas (NTSC-U)";
  case CONF_REGION_EU:
    return "Europe (PAL)";
  case CONF_REGION_KR:
    return "South Korea (NTSC-K)";
  case CONF_REGION_CN:
    return "China";
  default:
    return "Unknown";
  }
}

static const char *get_video_mode_string(void) {
  switch (CONF_GetVideo()) {
  case CONF_VIDEO_NTSC:
    return "NTSC (480i/480p)";
  case CONF_VIDEO_PAL:
    return "PAL (576i/480p)";
  case CONF_VIDEO_MPAL:
    return "MPAL (480i/480p)";
  default:
    return "Unknown";
  }
}

static const char *get_language_string(void) {
  switch (CONF_GetLanguage()) {
  case CONF_LANG_JAPANESE:
    return "Japanese";
  case CONF_LANG_ENGLISH:
    return "English";
  case CONF_LANG_GERMAN:
    return "German";
  case CONF_LANG_FRENCH:
    return "French";
  case CONF_LANG_SPANISH:
    return "Spanish";
  case CONF_LANG_ITALIAN:
    return "Italian";
  case CONF_LANG_DUTCH:
    return "Dutch";
  case CONF_LANG_SIMP_CHINESE:
    return "Simplified Chinese";
  case CONF_LANG_TRAD_CHINESE:
    return "Traditional Chinese";
  case CONF_LANG_KOREAN:
    return "Korean";
  default:
    return "Unknown";
  }
}

static const char *get_aspect_string(void) {
  switch (CONF_GetAspectRatio()) {
  case CONF_ASPECT_4_3:
    return "4:3 (Standard)";
  case CONF_ASPECT_16_9:
    return "16:9 (Widescreen)";
  default:
    return "Unknown";
  }
}

static const char *get_progressive_string(void) {
  s32 prog = CONF_GetProgressiveScan();
  if (prog > 0)
    return "Enabled";
  if (prog == 0)
    return "Disabled";
  return "Unknown";
}

/*---------------------------------------------------------------------------*/
void run_system_info(void) {
  u32 hollywood_ver = SYS_GetHollywoodRevision();
  u32 mem1_size = SYS_GetArena1Size();
  u32 mem2_size = SYS_GetArena2Size();
  s32 ios_ver = IOS_GetVersion();
  s32 ios_rev = IOS_GetRevision();
  u32 boot2_version = 0;
  s32 ret = ES_GetBoot2Version(&boot2_version);
  u32 device_id = 0;
  char buf[64];

  ES_GetDeviceID(&device_id);

  /* Display settings */
  ui_draw_kv("Console Region", get_region_string());
  ui_draw_kv("Video Standard", get_video_mode_string());
  ui_draw_kv("Display Language", get_language_string());
  ui_draw_kv("Aspect Ratio", get_aspect_string());
  ui_draw_kv("Progressive Scan", get_progressive_string());

  /* Hardware */
  ui_draw_section("Hardware");

  snprintf(buf, sizeof(buf), "0x%08X", hollywood_ver);
  ui_draw_kv("Hollywood Revision", buf);

  snprintf(buf, sizeof(buf), "%u", device_id);
  ui_draw_kv("Device ID", buf);

  if (ret >= 0) {
    snprintf(buf, sizeof(buf), "v%u", boot2_version);
    ui_draw_kv("Boot2 Version", buf);
    if (boot2_version >= 5)
      ui_draw_warn("Boot2v5+ - BootMii can only run as IOS");
  }

  /* Memory */
  ui_draw_section("Memory");

  snprintf(buf, sizeof(buf), "%u KB (%.1f MB)", mem1_size / 1024,
           (float)mem1_size / (1024.0f * 1024.0f));
  ui_draw_kv("MEM1 Arena Free", buf);

  snprintf(buf, sizeof(buf), "%u KB (%.1f MB)", mem2_size / 1024,
           (float)mem2_size / (1024.0f * 1024.0f));
  ui_draw_kv("MEM2 Arena Free", buf);

  ui_draw_kv("MEM1 Total", "24 MB (fixed)");
  ui_draw_kv("MEM2 Total", "64 MB (fixed)");

  /* Firmware */
  ui_draw_section("Firmware");

  snprintf(buf, sizeof(buf), "IOS%d (rev %d)", ios_ver, ios_rev);
  ui_draw_kv("Running IOS", buf);
  ui_draw_kv("CPU", "Broadway (IBM PowerPC 750CL)");
  ui_draw_kv("CPU Clock", "729 MHz (fixed)");
  ui_draw_kv("GPU", "Hollywood (ATI/AMD)");
  ui_draw_kv("GPU Clock", "243 MHz (fixed)");

  /* Brick Protection */
  ui_draw_section("Brick Protection");
  {
    bool has_priiloader = detect_priiloader();
    bool has_bootmii_boot2 = (ret >= 0 && boot2_version <= 4);
    bool has_bootmii_ios = detect_bootmii_ios();
    int protection_count = 0;

    if (has_priiloader) {
      ui_draw_kv_color("Priiloader", UI_BGREEN, "Installed");
      protection_count++;
    } else {
      ui_draw_kv_color("Priiloader", UI_BRED, "Not found");
    }

    if (has_bootmii_boot2) {
      ui_draw_kv_color("BootMii (boot2)", UI_BGREEN,
                       "Compatible (boot2 v4 or lower)");
      protection_count++;
    } else {
      ui_draw_kv_color("BootMii (boot2)", UI_BYELLOW,
                       "Not available (boot2 v5+)");
    }

    if (has_bootmii_ios) {
      ui_draw_kv_color("BootMii (IOS)", UI_BGREEN, "Installed");
      protection_count++;
    } else {
      ui_draw_kv_color("BootMii (IOS)", UI_BYELLOW, "Not found");
    }

    ui_printf("\n");
    if (protection_count >= 2) {
      ui_draw_ok("Brick protection: GOOD");
    } else if (protection_count == 1) {
      ui_draw_warn("Brick protection: PARTIAL - install more layers");
    } else {
      ui_draw_err("Brick protection: NONE - your Wii is at risk!");
      ui_draw_info("Install Priiloader and BootMii ASAP");
    }
  }

  ui_printf("\n");
  ui_draw_ok("System information collected successfully");
}

/*---------------------------------------------------------------------------*/
void get_system_info_report(char *buf, int bufsize) {
  u32 hollywood_ver = SYS_GetHollywoodRevision();
  u32 mem1_size = SYS_GetArena1Size();
  u32 mem2_size = SYS_GetArena2Size();
  s32 ios_ver = IOS_GetVersion();
  s32 ios_rev = IOS_GetRevision();
  u32 boot2_version = 0;
  u32 device_id = 0;

  ES_GetBoot2Version(&boot2_version);
  ES_GetDeviceID(&device_id);

  {
    bool has_priiloader = detect_priiloader();
    bool has_bootmii_boot2 = (boot2_version <= 4);
    bool has_bootmii_ios = detect_bootmii_ios();

    snprintf(buf, bufsize,
             "=== SYSTEM INFORMATION ===\n"
             "Region:              %s\n"
             "Video Standard:      %s\n"
             "Language:            %s\n"
             "Aspect Ratio:        %s\n"
             "Progressive Scan:    %s\n"
             "Hollywood Revision:  0x%08X\n"
             "Device ID:           %u\n"
             "Boot2 Version:       v%u\n"
             "Running IOS:         IOS%d (rev %d)\n"
             "MEM1 Arena Free:     %u KB\n"
             "MEM2 Arena Free:     %u KB\n"
             "\n"
             "--- Brick Protection ---\n"
             "Priiloader:          %s\n"
             "BootMii (boot2):     %s\n"
             "BootMii (IOS):       %s\n"
             "\n",
             get_region_string(), get_video_mode_string(),
             get_language_string(), get_aspect_string(),
             get_progressive_string(), hollywood_ver, device_id, boot2_version,
             ios_ver, ios_rev, mem1_size / 1024, mem2_size / 1024,
             has_priiloader ? "Installed" : "Not found",
             has_bootmii_boot2 ? "Compatible" : "Not available (boot2 v5+)",
             has_bootmii_ios ? "Installed" : "Not found");
  }
}
