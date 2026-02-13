/*
 * WiiMedic - recommendations.c
 * Scans all subsystems and generates actionable, beginner-friendly advice.
 * Each recommendation is categorized: CRITICAL / WARNING / TIP
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <gccore.h>
#include <ogc/isfs.h>
#include <fat.h>
#include <dirent.h>
#include <wiiuse/wpad.h>

#include "recommendations.h"
#include "ui_common.h"

#define MAX_RECS       32
#define REC_MSG_LEN    256
#define REPORT_SIZE    8192

typedef enum { SEV_CRITICAL, SEV_WARNING, SEV_TIP } severity_t;

typedef struct {
    severity_t sev;
    char       msg[REC_MSG_LEN];
    char       action[REC_MSG_LEN];
} rec_t;

static rec_t s_recs[MAX_RECS];
static int   s_rec_count = 0;
static char  s_report[REPORT_SIZE];

/*---------------------------------------------------------------------------*/
static void add_rec(severity_t sev, const char *msg, const char *action) {
    if (s_rec_count >= MAX_RECS) return;
    s_recs[s_rec_count].sev = sev;
    strncpy(s_recs[s_rec_count].msg, msg, REC_MSG_LEN - 1);
    strncpy(s_recs[s_rec_count].action, action, REC_MSG_LEN - 1);
    s_rec_count++;
}

/*---------------------------------------------------------------------------*/
/* Check: BootMii / NAND backup safety */
static void check_nand_safety(void) {
    u32 title_count = 0;
    u64 *tlist = NULL;
    u32 i;
    bool has_bootmii_ios = false;
    bool has_bootmii_boot2 = false;
    u32 boot2_version = 0;

    ES_GetBoot2Version(&boot2_version);

    if (ES_GetNumTitles(&title_count) >= 0 && title_count > 0) {
        tlist = (u64 *)memalign(32, title_count * sizeof(u64));
        if (tlist && ES_GetTitles(tlist, title_count) >= 0) {
            for (i = 0; i < title_count; i++) {
                u32 upper = (u32)(tlist[i] >> 32);
                u32 lower = (u32)(tlist[i] & 0xFFFFFFFF);
                if (upper == 1 && (lower == 254 || lower == 236))
                    has_bootmii_ios = true;
            }
        }
        if (tlist) free(tlist);
    }

    /* Check if boot2 version allows boot2 install */
    if (boot2_version < 5)
        has_bootmii_boot2 = true; /* Possible, not confirmed installed */

    /* Check for actual NAND backup file on SD */
    {
        FILE *f = fopen("sd:/nand.bin", "r");
        if (f) {
            fclose(f);
            /* They have a backup, good */
            add_rec(SEV_TIP, "NAND backup found on SD card (nand.bin)",
                    "Keep this backup safe - it's your recovery lifeline!");
        } else {
            add_rec(SEV_CRITICAL, "No NAND backup detected on SD card",
                    "Use BootMii to create a NAND backup ASAP. This is your only "
                    "way to recover from a brick. Google 'BootMii NAND backup guide'.");
        }
    }

    if (!has_bootmii_ios) {
        add_rec(SEV_CRITICAL, "BootMii IOS not detected",
                "Install BootMii via hackmii.com installer. Without it, a NAND "
                "backup and recovery is impossible. This is the #1 safety measure.");
    } else {
        if (has_bootmii_boot2)
            add_rec(SEV_TIP, "BootMii as boot2 may be available (Boot2 < v5)",
                    "BootMii as boot2 gives the best brick protection. Check if "
                    "it is installed via hackmii installer.");
        else
            add_rec(SEV_TIP, "BootMii IOS detected - good!",
                    "Your console has BootMii as IOS. Make sure you have a NAND "
                    "backup. boot2 install is not possible (Boot2 v5+).");
    }

    /* Check for Priiloader */
    {
        FILE *f = fopen("sd:/apps/priiloader/boot.dol", "r");
        if (f) {
            fclose(f);
            add_rec(SEV_TIP, "Priiloader found on SD",
                    "Priiloader adds System Menu level brick protection. "
                    "Make sure it is actually installed to NAND via its installer.");
        } else {
            add_rec(SEV_WARNING, "Priiloader not found on SD card",
                    "Install Priiloader for extra brick protection. It can save "
                    "you from banner bricks and bad updates. Get it from the "
                    "WiiBrew wiki.");
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Check: NAND filesystem health */
static void check_nand_health(void) {
    s32 ret;
    u32 used_bytes = 0, used_inodes = 0;
    float cluster_pct, inode_pct;
    int tmp_count, import_count;

    ret = ISFS_Initialize();
    if (ret < 0) {
        add_rec(SEV_WARNING, "Could not access NAND filesystem",
                "NAND may need a different IOS. Try running IOS Check first.");
        return;
    }

    ISFS_GetUsage("/", &used_bytes, &used_inodes);

    cluster_pct = (float)used_bytes * 100.0f / 2048.0f;
    inode_pct   = (float)used_inodes * 100.0f / 6143.0f;

    if (cluster_pct > 90.0f) {
        add_rec(SEV_CRITICAL, "NAND storage is over 90% full!",
                "Delete unused channels and save data via Wii Settings > "
                "Data Management. A full NAND can cause system instability "
                "and failed updates.");
    } else if (cluster_pct > 75.0f) {
        add_rec(SEV_WARNING, "NAND storage is getting full (over 75%)",
                "Consider removing unused channels to free space. Use "
                "Data Management in Wii Settings.");
    }

    if (inode_pct > 85.0f) {
        add_rec(SEV_WARNING, "NAND has many files (inode usage high)",
                "Too many small files on NAND. Remove unused save data "
                "or channels to free inodes.");
    }

    /* Check /import for interrupted installs */
    {
        char path[32] ATTRIBUTE_ALIGN(32);
        u32 count = 0;
        strcpy(path, "/import");
        import_count = 0;
        if (ISFS_ReadDir(path, NULL, &count) >= 0)
            import_count = (int)count;
    }
    if (import_count > 0) {
        add_rec(SEV_WARNING, "Interrupted installation detected (/import not empty)",
                "A channel or title install was interrupted. You may need to "
                "re-download the title or clean up with a NAND manager.");
    }

    /* Check /tmp */
    {
        char path[32] ATTRIBUTE_ALIGN(32);
        u32 count = 0;
        strcpy(path, "/tmp");
        tmp_count = 0;
        if (ISFS_ReadDir(path, NULL, &count) >= 0)
            tmp_count = (int)count;
    }
    if (tmp_count > 20) {
        add_rec(SEV_TIP, "NAND /tmp has many entries",
                "Lots of temp files may slow down operations. Usually harmless "
                "but can be cleaned with a NAND manager if needed.");
    }

    ISFS_Deinitialize();
}

/*---------------------------------------------------------------------------*/
/* Check: IOS setup */
static void check_ios_setup(void) {
    u32 title_count = 0;
    u64 *tlist = NULL;
    u32 i;
    int cios_count = 0;
    int stub_count = 0;
    int total_ios = 0;
    bool has_ios58 = false;
    bool has_ios36 __attribute__((unused)) = false;

    if (ES_GetNumTitles(&title_count) < 0 || title_count == 0) return;

    tlist = (u64 *)memalign(32, title_count * sizeof(u64));
    if (!tlist) return;
    if (ES_GetTitles(tlist, title_count) < 0) { free(tlist); return; }

    for (i = 0; i < title_count; i++) {
        u32 upper = (u32)(tlist[i] >> 32);
        u32 lower = (u32)(tlist[i] & 0xFFFFFFFF);
        u32 tmd_size = 0;

        if (upper != 1 || lower < 3 || lower > 255) continue;
        total_ios++;

        if (lower == 58) has_ios58 = true;
        if (lower == 36) has_ios36 = true;

        /* Check for stubs */
        if (ES_GetStoredTMDSize(tlist[i], &tmd_size) >= 0 && tmd_size > 0) {
            signed_blob *tmd_buf = (signed_blob *)memalign(32, tmd_size);
            if (tmd_buf) {
                if (ES_GetStoredTMD(tlist[i], tmd_buf, tmd_size) >= 0) {
                    tmd *t = SIGNATURE_PAYLOAD(tmd_buf);
                    if (t->title_version == 0 || t->num_contents == 0)
                        stub_count++;
                }
                free(tmd_buf);
            }
        }

        /* cIOS slots */
        if ((lower >= 222 && lower <= 223) ||
            (lower >= 249 && lower <= 251))
            cios_count++;
    }

    free(tlist);

    if (cios_count == 0) {
        add_rec(SEV_WARNING, "No cIOS detected (IOS249/250/etc.)",
                "Install d2x cIOS for USB loader support. Without cIOS, "
                "you cannot load games from USB. Use 'd2x cIOS Installer' "
                "from the Homebrew Browser.");
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "%d cIOS installation(s) found - USB loaders ready", cios_count);
        add_rec(SEV_TIP, buf, "Your cIOS setup looks good for USB loading.");
    }

    if (stub_count > 5) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%d stub IOS versions detected", stub_count);
        add_rec(SEV_TIP, buf,
                "Stub IOS are placeholders. Usually harmless but some games "
                "may need specific IOS versions restored.");
    }

    if (!has_ios58) {
        add_rec(SEV_WARNING, "IOS58 not found",
                "IOS58 is important for network features and some homebrew. "
                "Consider updating via NUS Downloader or online updater.");
    }
}

/*---------------------------------------------------------------------------*/
/* Check: Storage health */
static void check_storage(void) {
    DIR *sd_dir  = opendir("sd:/");
    DIR *usb_dir = opendir("usb:/");
    bool has_sd  = (sd_dir != NULL);
    bool has_usb = (usb_dir != NULL);

    if (sd_dir)  closedir(sd_dir);
    if (usb_dir) closedir(usb_dir);

    if (!has_sd && !has_usb) {
        add_rec(SEV_WARNING, "No SD card or USB drive detected",
                "Insert an SD card (FAT32) for homebrew. Many apps require "
                "SD or USB storage to function.");
    } else if (!has_sd) {
        add_rec(SEV_TIP, "No SD card detected",
                "While USB works for games, many homebrew apps and "
                "BootMii backups require an SD card. SDHC Class 10 recommended.");
    }

    /* Check for apps directory */
    if (has_sd) {
        DIR *apps = opendir("sd:/apps");
        if (!apps) {
            add_rec(SEV_TIP, "No /apps folder on SD card",
                    "Create an 'apps' folder on your SD card root. "
                    "This is where the Homebrew Channel looks for apps.");
        } else {
            closedir(apps);
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Check: Controller state */
static void check_controllers(void) {
    int warmup, chan, port;
    int gc_count = 0, wii_count = 0;

    PAD_ScanPads();
    for (port = 0; port < 4; port++) {
        s16 sx = PAD_StickX(port);
        s16 sy = PAD_StickY(port);
        u16 bt = PAD_ButtonsHeld(port);
        if (sx || sy || bt || PAD_TriggerL(port) || PAD_TriggerR(port))
            gc_count++;
    }

    for (warmup = 0; warmup < 30; warmup++) {
        WPAD_ScanPads();
        VIDEO_WaitVSync();
    }
    for (chan = 0; chan < 4; chan++) {
        u32 type;
        if (WPAD_Probe(chan, &type) == WPAD_ERR_NONE) {
            wii_count++;
            /* Check battery on connected remotes */
            WPADData *wd = WPAD_Data(chan);
            if (wd) {
                float batt = (float)wd->battery_level / 2.08f;
                if (batt < 15.0f) {
                    char buf[128];
                    snprintf(buf, sizeof(buf),
                             "Wii Remote %d battery is very low (%.0f%%)", chan + 1, batt);
                    add_rec(SEV_WARNING, buf,
                            "Replace or recharge the batteries soon to avoid "
                            "losing sync during gameplay.");
                }
            }
        }
    }

    /* GC drift is checked in controller_test module, skip here */
    (void)gc_count;
}

/*---------------------------------------------------------------------------*/
void run_recommendations(void) {
    int i, crits = 0, warns = 0, tips = 0;

    s_rec_count = 0;
    memset(s_report, 0, sizeof(s_report));

    ui_draw_info("Running full system checkup...");
    ui_printf("\n");

    ui_printf(UI_WHITE "   Checking NAND backup safety...\n" UI_RESET);
    check_nand_safety();

    ui_printf(UI_WHITE "   Checking NAND health...\n" UI_RESET);
    check_nand_health();

    ui_printf(UI_WHITE "   Checking IOS setup...\n" UI_RESET);
    check_ios_setup();

    ui_printf(UI_WHITE "   Checking storage devices...\n" UI_RESET);
    check_storage();

    ui_printf(UI_WHITE "   Checking controllers...\n" UI_RESET);
    check_controllers();

    /* Tally */
    for (i = 0; i < s_rec_count; i++) {
        switch (s_recs[i].sev) {
            case SEV_CRITICAL: crits++; break;
            case SEV_WARNING:  warns++; break;
            case SEV_TIP:      tips++;  break;
        }
    }

    ui_printf("\n");

    /* Overall status */
    {
        char buf[128];
        if (crits > 0) {
            snprintf(buf, sizeof(buf),
                     "Found %d critical issue(s) that need attention!", crits);
            ui_draw_err(buf);
        } else if (warns > 0) {
            snprintf(buf, sizeof(buf),
                     "No critical issues, but %d warning(s) found.", warns);
            ui_draw_warn(buf);
        } else {
            ui_draw_ok("Your Wii is in good shape! No issues found.");
        }

        snprintf(buf, sizeof(buf),
                 "Results: %d critical, %d warnings, %d tips",
                 crits, warns, tips);
        ui_draw_info(buf);
    }

    ui_draw_line();
    ui_printf("\n");

    /* Print CRITICAL items first, then WARNING, then TIP */
    if (crits > 0) {
        ui_printf(UI_BRED "   === CRITICAL ISSUES ===\n\n" UI_RESET);
        for (i = 0; i < s_rec_count; i++) {
            if (s_recs[i].sev != SEV_CRITICAL) continue;
            ui_printf(UI_BRED "   [!!] %s\n" UI_RESET, s_recs[i].msg);
            ui_printf(UI_WHITE "        -> %s\n\n" UI_RESET, s_recs[i].action);
        }
    }

    if (warns > 0) {
        ui_printf(UI_BYELLOW "   === WARNINGS ===\n\n" UI_RESET);
        for (i = 0; i < s_rec_count; i++) {
            if (s_recs[i].sev != SEV_WARNING) continue;
            ui_printf(UI_BYELLOW "   [!]  %s\n" UI_RESET, s_recs[i].msg);
            ui_printf(UI_WHITE "        -> %s\n\n" UI_RESET, s_recs[i].action);
        }
    }

    if (tips > 0) {
        ui_printf(UI_BCYAN "   === TIPS ===\n\n" UI_RESET);
        for (i = 0; i < s_rec_count; i++) {
            if (s_recs[i].sev != SEV_TIP) continue;
            ui_printf(UI_BCYAN "   (i)  %s\n" UI_RESET, s_recs[i].msg);
            ui_printf(UI_WHITE "        -> %s\n\n" UI_RESET, s_recs[i].action);
        }
    }

    ui_draw_line();
    ui_printf("\n");
    ui_draw_ok("System checkup complete");
}

/*---------------------------------------------------------------------------*/
void get_recommendations_report(char *buf, int bufsize) {
    int i, pos = 0;

    pos += snprintf(buf + pos, bufsize - pos,
        "=== AUTO-DETECT RECOMMENDATIONS ===\n");

    for (i = 0; i < s_rec_count; i++) {
        const char *sev_str;
        switch (s_recs[i].sev) {
            case SEV_CRITICAL: sev_str = "CRITICAL"; break;
            case SEV_WARNING:  sev_str = "WARNING";  break;
            case SEV_TIP:      sev_str = "TIP";      break;
            default:           sev_str = "???";      break;
        }
        pos += snprintf(buf + pos, bufsize - pos,
            "[%s] %s\n  -> %s\n\n",
            sev_str, s_recs[i].msg, s_recs[i].action);
    }
}
