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
#include <unistd.h>

#include "system_info.h"
#include "ui_common.h"

/*---------------------------------------------------------------------------*/

#define SM_ID (u64)0x0000000100000002ULL

/* ISFS error code for already initialized */
#define ISFS_EALREADY -105

/*
 * Helper to get the System Menu boot content ID by parsing its TMD.
 * Inspired by Priiloader, thanks DacoTaco
 * Returns the CID on success, 0 on failure.
 */
static u32 get_SM_boot_content_id(void) {
  u32 tmd_size = 0;
  if (ES_GetStoredTMDSize(SM_ID, &tmd_size) < 0 || tmd_size == 0)
    return 0;

  signed_blob *stmd = (signed_blob *)memalign(32, (tmd_size + 31) & ~31);
  if (!stmd)
    return 0;

  u32 content_id = 0;
  if (ES_GetStoredTMD(SM_ID, stmd, tmd_size) >= 0) {
    tmd *tmd_ptr = (tmd *)SIGNATURE_PAYLOAD(stmd);
    u16 boot_index = tmd_ptr->boot_index;

    /* Find the content matching the boot index */
    for (u16 i = 0; i < tmd_ptr->num_contents; i++) {
      if (tmd_ptr->contents[i].index == boot_index) {
        content_id = tmd_ptr->contents[i].cid;
        break;
      }
    }
  }

  free(stmd);
  return content_id;
}

/*
 * Helper to check for Priiloader presence.
 * Returns true if any Priiloader-related files or the backup System Menu are
 * found.
 */
static bool detect_priiloader(void) {
  u32 content_id = get_SM_boot_content_id();
  if (content_id == 0)
    return false;

  /* Handle ISFS state. If already initialized, ISFS_Initialize returns < 0. */
  s32 isfs_res = ISFS_Initialize();
  bool we_initialized_isfs = (isfs_res >= 0);

  if (isfs_res < 0 && isfs_res != ISFS_EALREADY) {
    return false;
  }

  bool detected = false;
  char path[ISFS_MAXPATH];

  /* 1. Check for data configuration files (loader.ini is the definitive marker)
   */
  const char *data_files[] = {"/title/00000001/00000002/data/loader.ini",
                              "/title/00000001/00000002/data/setting.ini"};
  for (int j = 0; j < 2; j++) {
    s32 fd = ISFS_Open(data_files[j], ISFS_OPEN_READ);
    if (fd >= 0) {
      detected = true;
      ISFS_Close(fd);
      break;
    }
  }

  /* 2. Check for the backup System Menu .app file (created by Priiloader) */
  if (!detected) {
    snprintf(path, sizeof(path), "/title/00000001/00000002/content/%08x.app",
             (unsigned int)(content_id + 0x10000000));
    s32 fd_backup = ISFS_Open(path, ISFS_OPEN_READ);
    if (fd_backup >= 0) {
      detected = true;
      ISFS_Close(fd_backup);
    }
  }

  if (we_initialized_isfs) {
    ISFS_Deinitialize();
  }

  return detected;
}

/*
 * Scans the active System Menu binary (Priiloader) for its version string.
 * Special thanks and credit to Abdelali221 for creating the version shower!
 */
static void get_priiloader_version(char *out_version, int max_len) {
  strncpy(out_version, "Unknown", max_len);

  u32 content_id = get_SM_boot_content_id();
  if (content_id == 0)
    return;

  s32 isfs_res = ISFS_Initialize();
  bool we_initialized_isfs = (isfs_res >= 0);

  char path[ISFS_MAXPATH];
  snprintf(path, sizeof(path), "/title/00000001/00000002/content/%08x.app",
           (unsigned int)content_id);
  s32 fd = ISFS_Open(path, ISFS_OPEN_READ);
  if (fd >= 0) {
    u32 buf_size = 4096;
    u8 *buf = memalign(32, buf_size);
    if (buf) {
      s32 read_bytes;
      bool found = false;
      while (!found && (read_bytes = ISFS_Read(fd, buf, buf_size)) > 0) {
        /* Scan for "0.10.x" or "v0.x" strings representing priiloader versions
         */
        for (int i = 0; i < read_bytes - 12; i++) {
          if ((buf[i] == '0' && buf[i + 1] == '.' && buf[i + 2] == '1' &&
               buf[i + 3] == '0' && buf[i + 4] == '.') ||
              (buf[i] == 'v' && buf[i + 1] == '0' && buf[i + 2] == '.')) {
            strncpy(out_version, (char *)&buf[i], max_len - 1);
            out_version[max_len - 1] = '\0';
            for (int j = 0; j < strlen(out_version); j++) {
              if (out_version[j] < 0x20 || out_version[j] > 0x7E) {
                out_version[j] = '\0';
                break;
              }
            }
            if (strlen(out_version) > 3) {
              found = true;
              break;
            }
          }
        }
      }
      free(buf);
    }
    ISFS_Close(fd);
  }

  if (we_initialized_isfs) {
    ISFS_Deinitialize();
  }
}

static bool detect_bootmii_ios(void) {
  u32 tmd_size = 0;
  u64 tid_254 = 0x00000001000000FEULL; /* IOS254 */
  if (ES_GetStoredTMDSize(tid_254, &tmd_size) >= 0 && tmd_size > 0)
    return true;

  return false;
}

/* PPC Hollywood regs */
#define HW_REG_BASE 0xCD000000
#define HW_AHBPROT_OFF 0x064
#define HW_OTPCMD_OFF 0x1ec
#define HW_OTPDATA_OFF 0x1f0
#define OTP_RD_BIT (1U << 31)

static const u8 boot1a_hash[20] = {0xb3, 0x0c, 0x32, 0xb9, 0x62, 0xc7, 0xcd,
                                   0x08, 0xab, 0xe3, 0x3d, 0x01, 0x5b, 0x9b,
                                   0x8b, 0x1d, 0xb1, 0x09, 0x75, 0x44};
static const u8 boot1b_hash[20] = {0xef, 0x3e, 0xf7, 0x81, 0x09, 0x60, 0x8d,
                                   0x56, 0xdf, 0x56, 0x79, 0xa6, 0xf9, 0x2e,
                                   0x13, 0xf7, 0x8b, 0xbd, 0xdf, 0xdf};
static const u8 boot1c_hash[20] = {0xd2, 0x20, 0xc8, 0xa4, 0x86, 0xc6, 0x31,
                                   0xd0, 0xdf, 0x5a, 0xdb, 0x31, 0x96, 0xec,
                                   0xbc, 0x66, 0x87, 0x80, 0xcc, 0x8d};
static const u8 boot1d_hash[20] = {0xf7, 0x93, 0x06, 0x8a, 0x09, 0xe8, 0x09,
                                   0x86, 0xe2, 0xa0, 0x23, 0xc0, 0xc2, 0x3f,
                                   0x06, 0x14, 0x0e, 0xd1, 0x69, 0x74};

static int get_boot1_bootmii_compatible(void) {
  volatile u32 *hw = (volatile u32 *)HW_REG_BASE;

  if (hw[HW_AHBPROT_OFF / 4] != 0xFFFFFFFF)
    return -1;

  u8 hash[20];
  unsigned int i;
  for (i = 0; i < 5; i++) {
    hw[HW_OTPCMD_OFF / 4] = OTP_RD_BIT | i;

    /* FIX: Add timeout to prevent infinite hang */
    u32 timeout = 10000;
    while ((hw[HW_OTPCMD_OFF / 4] & OTP_RD_BIT) && timeout > 0) {
      usleep(100);
      timeout--;
    }
    if (timeout == 0)
      return -1; // Read failed

    u32 word = hw[HW_OTPDATA_OFF / 4];
    hash[i * 4 + 0] = (u8)(word >> 24);
    hash[i * 4 + 1] = (u8)(word >> 16);
    hash[i * 4 + 2] = (u8)(word >> 8);
    hash[i * 4 + 3] = (u8)word;
  }

  if (memcmp(hash, boot1a_hash, 20) == 0 || memcmp(hash, boot1b_hash, 20) == 0)
    return 1; /* compatible */
  if (memcmp(hash, boot1c_hash, 20) == 0 || memcmp(hash, boot1d_hash, 20) == 0)
    return 0; /* not compatible */
  return 2;   /* unknown */
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

  /* Boot2 version */
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

  ui_draw_section("Brick Protection");
  {
    bool has_priiloader = detect_priiloader();
    int boot1_ok = get_boot1_bootmii_compatible();
    bool boot2_suggests_bootmii_ok = (ret >= 0 && boot2_version <= 4);
    bool has_bootmii_ios = detect_bootmii_ios();
    int protection_count = 0;

    if (has_priiloader) {
      ui_draw_kv_color("Priiloader", UI_BGREEN, "Installed");
      char p_ver[32];
      get_priiloader_version(p_ver, sizeof(p_ver));
      ui_printf("   " UI_BCYAN "(i) " UI_WHITE " %s\n" UI_RESET, p_ver);
      protection_count++;
    } else {
      ui_draw_kv_color("Priiloader", UI_BRED, "Not found");
    }

    if (hollywood_ver >= 0x21) {
      ui_draw_kv_color("BootMii (boot2)", UI_BYELLOW,
                       "Not compatible (Late HW)");
    } else if (boot1_ok == 1) {
      ui_draw_kv_color("BootMii (boot2)", UI_BGREEN, "Compatible (boot1a/b)");
      protection_count++;
    } else if (boot1_ok == 0) {
      ui_draw_kv_color("BootMii (boot2)", UI_BYELLOW,
                       "Not compatible (boot1c/d)");
    } else if (boot1_ok == 2) {
      ui_draw_kv_color("BootMii (boot2)", UI_BYELLOW, "Unknown boot1 revision");
    } else {
      /* Fallback logic if OTP read failed */
      if (boot2_suggests_bootmii_ok) {
        ui_draw_kv_color("BootMii (boot2)", UI_BGREEN,
                         "Likely compatible (boot2 proxy)");
        protection_count++;
      } else {
        ui_draw_kv_color("BootMii (boot2)", UI_BYELLOW, "Likely not");
      }
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
      ui_draw_warn("Brick protection: PARTIAL");
    } else {
      ui_draw_err("Brick protection: NONE");
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
    bool has_bootmii_ios = detect_bootmii_ios();

    /* Logic correction: boot1 compatible means boot2 install is possible */
    bool has_bootmii_boot2 = (boot1_ok == 1);

    char p_ver[32] = "";
    if (has_priiloader) {
      get_priiloader_version(p_ver, sizeof(p_ver));
    }
    char prii_str[64];
    if (has_priiloader) {
      snprintf(prii_str, sizeof(prii_str), "Installed (%s)", p_ver);
    } else {
      strcpy(prii_str, "Not found");
    }

    const char *boot2_str;
    if (hollywood_ver >= 0x21)
      boot2_str = "Not compatible (Late HW)";
    else if (boot1_ok == 1)
      boot2_str = "Compatible (boot1a/b)";
    else if (boot1_ok == 0)
      boot2_str = "Not compatible (boot1c/d)";
    else if (boot1_ok == 2)
      boot2_str = "Unknown boot1 revision";
    else
      boot2_str = boot2_suggests_ok ? "Likely compatible (boot2 proxy)"
                                    : "Likely not (boot2 v5+)";

    const char *rating =
        (has_priiloader && (has_bootmii_boot2 || has_bootmii_ios)) ? "GOOD"
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
             ios_ver, ios_rev, mem1_size / 1024, mem2_size / 1024, prii_str,
             boot2_str, has_bootmii_ios ? "Installed" : "Not found", rating);
  }
}