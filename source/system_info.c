/*
 * WiiMedic - system_info.c
 * Displays comprehensive system hardware and firmware information
 */

#include <dirent.h>
#include <gccore.h>
#include <malloc.h>
#include <ogc/isfs.h>
#include <ogc/machine/processor.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "system_info.h"
#include "ui_common.h"

#define SM_ID (u64)0x0000000100000002

static u8 tmdbuff[MAX_SIGNED_TMD_SIZE] ATTRIBUTE_ALIGN(64);

/*---------------------------------------------------------------------------*/
/* Brick protection detection helpers                                        */
/*---------------------------------------------------------------------------*/
/* Check for Priiloader app folder on SD/USB (multiple path and case variants). */
static bool detect_priiloader_folder(void) {
  static const char *paths[] = {
    "sd:/apps/priiloader",
    "sd:/apps/Priiloader",
    "usb:/apps/priiloader",
    "usb:/apps/Priiloader",
  };
  unsigned int i;
  for (i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
    DIR *d = opendir(paths[i]);
    if (d) {
      closedir(d);
      return true;
    }
  }
  return false;
}

/*
 * Priiloader installs to NAND by adding a modified system menu .app;
 * the original is often backed up as 1000000XX.app in the title content dir.
 * Try multiple regions and content IDs. ISFS init/deinit here (NAND health may run later).
 */

// Inspired by Priiloader, thanks DacoTaco

static u32 get_SM_boot_content_id() {
  u32 tmdsz = 0;
  s32 ret = ES_GetStoredTMDSize(SM_ID, &tmdsz);

  if(ret < 0) return false;

  signed_blob* tmdblob = (signed_blob*)tmdbuff;

  memset(tmdbuff, 0, tmdsz);

  ret = ES_GetStoredTMD(SM_ID, tmdblob, tmdsz);

  if(ret < 0) return false;

  tmd* tmdptr = (tmd*)(SIGNATURE_PAYLOAD(tmdblob));

  u32 id = 0;
	for(u16 i = 0; i < tmdptr->num_contents; ++i)
	{
		if (tmdptr->contents[i].index == tmdptr->boot_index)
		{
			id = tmdptr->contents[i].cid;
			break;
		}
	}

  if(id == 0) return -1;
  return id;
}

static bool detect_priiloader_nand(void) {
  char path[256];

  sprintf(path, "/title/00000001/00000002/content/%x.app", get_SM_boot_content_id() + 0x10000000);

  int ret = ISFS_Initialize();
  if (ret < 0)
    return false;

  s32 fd = ISFS_Open(path, ISFS_OPEN_READ);
  if (fd >= 0) {
    ISFS_Close(fd);
    ISFS_Deinitialize();
    return true;
  }
  return false;
}

/* Combined: NAND install (best) or app folder on current SD/USB. */
static bool detect_priiloader(void) {
  if (detect_priiloader_folder())
    return true;
  return detect_priiloader_nand();
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

/*
 * BootMii-as-boot2 compatibility is determined by boot1 (boot1a/b = yes, boot1c/d = no).
 * Read OTP boot1 hash via Hollywood registers when PPC has AHBPROT (e.g. launched from HBC).
 * Returns: 1 = compatible (boot1a/b), 0 = not compatible (boot1c/d), 2 = unknown hash, -1 = OTP not readable.
 */
/* PPC Hollywood regs: 0xCD000000 maps to 0x0D000000 (Wii map); 0x0D0xxx mirrors 0x0D8xxx for OTP */
#define HW_REG_BASE_PHYS  0xCD000000
#define HW_AHBPROT_OFF    0x064
#define HW_OTPCMD_OFF     0x1ec
#define HW_OTPDATA_OFF    0x1f0
#define OTP_RD_BIT        (1U << 31)

/* Known boot1 SHA1 hashes (20 bytes each) from WiiBrew Boot1 page */
static const u8 boot1a_hash[20] = {
  0xb3,0x0c,0x32,0xb9,0x62,0xc7,0xcd,0x08,0xab,0xe3,0x3d,0x01,0x5b,0x9b,0x8b,0x1d,0xb1,0x09,0x75,0x44
};
static const u8 boot1b_hash[20] = {
  0xef,0x3e,0xf7,0x81,0x09,0x60,0x8d,0x56,0xdf,0x56,0x79,0xa6,0xf9,0x2e,0x13,0xf7,0x8b,0xbd,0xdf,0xdf
};
static const u8 boot1c_hash[20] = {
  0xd2,0x20,0xc8,0xa4,0x86,0xc6,0x31,0xd0,0xdf,0x5a,0xdb,0x31,0x96,0xec,0xbc,0x66,0x87,0x80,0xcc,0x8d
};
static const u8 boot1d_hash[20] = {
  0xf7,0x93,0x06,0x8a,0x09,0xe8,0x09,0x86,0xe2,0xa0,0x23,0xc0,0xc2,0x3f,0x06,0x14,0x0e,0xd1,0x69,0x74
};

static int get_boot1_bootmii_compatible(void) {
  volatile u32 *hw = (volatile u32 *)HW_REG_BASE_PHYS;
  u32 ahb = hw[HW_AHBPROT_OFF / 4];
  /* PPCKERN (bit 31) = PPC full access to Hollywood regs; need it to read OTP */
  if (!(ahb & OTP_RD_BIT))
    return -1;
  /* Read OTP words 0-4 (boot1 hash, 20 bytes). RD=1, ADDR=word index */
  u8 hash[20];
  unsigned int i;
  for (i = 0; i < 5; i++) {
    hw[HW_OTPCMD_OFF / 4] = OTP_RD_BIT | i;
    u32 word = hw[HW_OTPDATA_OFF / 4];
    hash[i * 4 + 0] = (u8)(word >> 24);
    hash[i * 4 + 1] = (u8)(word >> 16);
    hash[i * 4 + 2] = (u8)(word >> 8);
    hash[i * 4 + 3] = (u8)word;
  }
  if (memcmp(hash, boot1a_hash, 20) == 0 || memcmp(hash, boot1b_hash, 20) == 0)
    return 1;  /* compatible */
  if (memcmp(hash, boot1c_hash, 20) == 0 || memcmp(hash, boot1d_hash, 20) == 0)
    return 0;  /* not compatible */
  return 2;    /* unknown */
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

  /* Boot2 version: shown for reference. BootMii-as-boot2 compatibility is
   * actually determined by boot1 revision (boot1a/b = can install, boot1c/d =
   * cannot). Boot1 is not exposed by the system API; we use boot2 version as
   * a proxy (v4 or lower ~ old boot1, v5+ ~ new boot1). */
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

  /* Brick Protection. BootMii (boot2): use boot1 from OTP when AHBPROT available,
   * else fall back to boot2 version proxy. */
  ui_draw_section("Brick Protection");
  {
    bool has_priiloader = detect_priiloader();
    int boot1_ok = get_boot1_bootmii_compatible();
    bool boot2_suggests_bootmii_ok = (ret >= 0 && boot2_version <= 4);
    bool has_bootmii_boot2 = (boot1_ok == 1) || (boot1_ok < 0 && boot2_suggests_bootmii_ok);
    bool has_bootmii_ios = detect_bootmii_ios();
    int protection_count = 0;

    if (has_priiloader) {
      ui_draw_kv_color("Priiloader", UI_BGREEN, "Installed");
      protection_count++;
    } else {
      ui_draw_kv_color("Priiloader", UI_BRED, "Not found");
      ui_draw_info("If installed in NAND, add apps/priiloader to this SD/USB to detect");
    }

    if (boot1_ok == 1) {
      ui_draw_kv_color("BootMii (boot2)", UI_BGREEN,
                       "Compatible (boot1a/b)");
      protection_count++;
    } else if (boot1_ok == 0) {
      ui_draw_kv_color("BootMii (boot2)", UI_BYELLOW,
                       "Not compatible (boot1c/d)");
    } else if (boot1_ok == 2) {
      ui_draw_kv_color("BootMii (boot2)", UI_BYELLOW,
                       "Unknown boot1 revision");
    } else {
      if (boot2_suggests_bootmii_ok) {
        ui_draw_kv_color("BootMii (boot2)", UI_BGREEN,
                         "Likely compatible (boot2 v4-; OTP not readable)");
        protection_count++;
      } else {
        ui_draw_kv_color("BootMii (boot2)", UI_BYELLOW,
                         "Likely not (boot2 v5+; OTP not readable)");
      }
      ui_draw_info("Run from HBC for boot1 check; using boot2 as proxy");
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
  s32 boot2_ret;

  boot2_ret = ES_GetBoot2Version(&boot2_version);
  ES_GetDeviceID(&device_id);

  {
    bool has_priiloader = detect_priiloader();
    int boot1_ok = get_boot1_bootmii_compatible();
    bool boot2_suggests_ok = (boot2_ret >= 0 && boot2_version <= 4);
    bool has_bootmii_boot2 = (boot1_ok == 1) || (boot1_ok < 0 && boot2_suggests_ok);
    bool has_bootmii_ios = detect_bootmii_ios();

    const char *boot2_str;
    if (boot1_ok == 1) boot2_str = "Compatible (boot1a/b)";
    else if (boot1_ok == 0) boot2_str = "Not compatible (boot1c/d)";
    else if (boot1_ok == 2) boot2_str = "Unknown boot1 revision";
    else boot2_str = boot2_suggests_ok ? "Likely compatible (boot2 proxy)" : "Likely not (boot2 v5+)";

    {
      const char *rating = (has_priiloader && (has_bootmii_boot2 || has_bootmii_ios)) ? "GOOD"
                         : (has_priiloader || has_bootmii_ios || has_bootmii_boot2) ? "PARTIAL"
                         : "NONE";
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
               "Protection Rating:   %s\n"
               "\n",
               get_region_string(), get_video_mode_string(),
               get_language_string(), get_aspect_string(),
               get_progressive_string(), hollywood_ver, device_id, boot2_version,
               ios_ver, ios_rev, mem1_size / 1024, mem2_size / 1024,
               has_priiloader ? "Installed" : "Not found",
               boot2_str,
               has_bootmii_ios ? "Installed" : "Not found",
               rating);
    }
  }
}
