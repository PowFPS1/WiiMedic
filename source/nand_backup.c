/*
 * WiiMedic - nand_backup.c
 * Checks whether the user has a NAND backup and the tools to create/restore one.
 * Designed as a safety reminder - this is the most important thing you can do
 * to protect a modded Wii from bricking.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <gccore.h>
#include <fat.h>
#include <sys/stat.h>
#include <dirent.h>

#include "nand_backup.h"
#include "ui_common.h"

static char s_report[4096] __attribute__((unused));

/* Status flags */
static bool s_has_bootmii_ios  = false;
static bool s_can_boot2        = false;
static bool s_has_priiloader   = false;
static bool s_has_nand_backup  = false;
static long s_backup_size      = 0;
static bool s_has_keys_bin     = false;
static int  s_safety_score     = 0;

/*---------------------------------------------------------------------------*/
static long get_file_size(const char *path) {
    FILE *f = fopen(path, "rb");
    long sz;
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fclose(f);
    return sz;
}

/*---------------------------------------------------------------------------*/
static void check_bootmii(void) {
    u32 title_count = 0;
    u64 *tlist = NULL;
    u32 i;
    u32 boot2_ver = 0;

    s_has_bootmii_ios = false;
    s_can_boot2 = false;

    ES_GetBoot2Version(&boot2_ver);
    if (boot2_ver < 5)
        s_can_boot2 = true;

    if (ES_GetNumTitles(&title_count) >= 0 && title_count > 0) {
        tlist = (u64 *)memalign(32, title_count * sizeof(u64));
        if (tlist && ES_GetTitles(tlist, title_count) >= 0) {
            for (i = 0; i < title_count; i++) {
                u32 upper = (u32)(tlist[i] >> 32);
                u32 lower = (u32)(tlist[i] & 0xFFFFFFFF);
                if (upper == 1 && (lower == 254 || lower == 236))
                    s_has_bootmii_ios = true;
            }
        }
        if (tlist) free(tlist);
    }
}

/*---------------------------------------------------------------------------*/
static void check_backup_files(void) {
    long sz;

    s_has_nand_backup = false;
    s_backup_size = 0;
    s_has_keys_bin = false;

    /* Check common nand.bin locations */
    sz = get_file_size("sd:/nand.bin");
    if (sz > 0) {
        s_has_nand_backup = true;
        s_backup_size = sz;
    }

    /* Also check BootMii directory */
    if (!s_has_nand_backup) {
        sz = get_file_size("sd:/bootmii/nand.bin");
        if (sz > 0) {
            s_has_nand_backup = true;
            s_backup_size = sz;
        }
    }

    /* Check on USB too */
    if (!s_has_nand_backup) {
        sz = get_file_size("usb:/nand.bin");
        if (sz > 0) {
            s_has_nand_backup = true;
            s_backup_size = sz;
        }
    }

    /* Check for keys.bin (needed for NAND restore) */
    if (get_file_size("sd:/keys.bin") > 0 ||
        get_file_size("sd:/bootmii/keys.bin") > 0)
        s_has_keys_bin = true;
}

/*---------------------------------------------------------------------------*/
static void check_priiloader(void) {
    s_has_priiloader = false;

    /* Priiloader is installed on NAND, but we can check if the app
       exists on SD as an indicator the user has it */
    if (get_file_size("sd:/apps/priiloader/boot.dol") > 0)
        s_has_priiloader = true;
}

/*---------------------------------------------------------------------------*/
static void calculate_safety_score(void) {
    s_safety_score = 0;

    /* NAND backup is the biggest factor */
    if (s_has_nand_backup)  s_safety_score += 35;
    if (s_has_keys_bin)     s_safety_score += 10;

    /* BootMii lets you create/restore backups */
    if (s_has_bootmii_ios)  s_safety_score += 25;
    if (s_can_boot2)        s_safety_score += 10;

    /* Priiloader is extra protection */
    if (s_has_priiloader)   s_safety_score += 20;
}

/*---------------------------------------------------------------------------*/
void run_nand_backup_check(void) {
    char buf[128];

    ui_draw_info("Checking your Wii's brick protection setup...");
    ui_printf("\n");

    /* Run all checks */
    ui_printf(UI_WHITE "   Checking BootMii installation...\n" UI_RESET);
    check_bootmii();

    ui_printf(UI_WHITE "   Scanning for NAND backup files...\n" UI_RESET);
    check_backup_files();

    ui_printf(UI_WHITE "   Checking for Priiloader...\n" UI_RESET);
    check_priiloader();

    calculate_safety_score();

    /* === Results === */

    /* Safety Score */
    ui_draw_section("Brick Protection Score");
    {
        const char *color;
        const char *grade;
        if (s_safety_score >= 80) {
            color = UI_BGREEN; grade = "EXCELLENT";
        } else if (s_safety_score >= 60) {
            color = UI_BGREEN; grade = "GOOD";
        } else if (s_safety_score >= 40) {
            color = UI_BYELLOW; grade = "FAIR";
        } else if (s_safety_score >= 20) {
            color = UI_BRED; grade = "POOR";
        } else {
            color = UI_BRED; grade = "CRITICAL - AT RISK";
        }

        snprintf(buf, sizeof(buf), "%d / 100 - %s", s_safety_score, grade);
        ui_draw_kv_color("Safety Score", color, buf);
    }
    ui_printf("\n");
    ui_draw_bar(s_safety_score, 100, 40);

    /* BootMii Status */
    ui_draw_section("BootMii Status");

    if (s_has_bootmii_ios) {
        ui_draw_ok("BootMii IOS installed (IOS254 or IOS236)");
        ui_draw_info("BootMii can create and restore NAND backups");

        if (s_can_boot2) {
            ui_draw_ok("Boot2 version allows boot2 installation");
            ui_draw_info("BootMii as boot2 = best brick protection possible");
        } else {
            ui_draw_warn("Boot2 v5+ - BootMii cannot be installed as boot2");
            ui_draw_info("BootMii as IOS still works but cannot recover from");
            ui_draw_info("all types of bricks. Priiloader recommended as backup.");
        }
    } else {
        ui_draw_err("BootMii IOS NOT detected!");
        ui_printf("\n");
        ui_draw_info("Without BootMii, you CANNOT create a NAND backup.");
        ui_draw_info("This is the most important safety tool for a modded Wii.");
        ui_printf("\n");
        ui_draw_info("How to install:");
        ui_draw_info("  1. Download HackMii Installer (hackmii.com)");
        ui_draw_info("  2. Put it on your SD card");
        ui_draw_info("  3. Launch via Homebrew Channel");
        ui_draw_info("  4. Select 'Install BootMii'");
    }

    /* NAND Backup Status */
    ui_draw_section("NAND Backup");

    if (s_has_nand_backup) {
        ui_draw_ok("NAND backup file found!");

        snprintf(buf, sizeof(buf), "%.1f MB", (float)s_backup_size / (1024.0f * 1024.0f));
        ui_draw_kv("Backup Size", buf);

        if (s_backup_size >= 512 * 1024 * 1024) {
            ui_draw_ok("Size looks correct for a full NAND dump (512+ MB)");
        } else {
            ui_draw_warn("Backup seems small - might be incomplete");
            ui_draw_info("A full Wii NAND backup should be ~528 MB");
        }

        if (s_has_keys_bin) {
            ui_draw_ok("keys.bin found - needed for NAND restore");
        } else {
            ui_draw_warn("keys.bin NOT found!");
            ui_draw_info("Without keys.bin, NAND cannot be restored.");
            ui_draw_info("Re-run BootMii backup to generate both files.");
        }
    } else {
        ui_draw_err("NO NAND backup found!");
        ui_printf("\n");
        ui_printf(UI_BRED
            "   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
            "   !!  YOUR WII HAS NO NAND BACKUP - HIGH RISK!   !!\n"
            "   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
            UI_RESET);
        ui_printf("\n");
        ui_draw_info("If your NAND gets corrupted, your Wii will BRICK");
        ui_draw_info("with no way to recover (without a hardware mod).");
        ui_printf("\n");
        ui_draw_info("How to create a NAND backup:");
        ui_draw_info("  1. Make sure BootMii is installed (see above)");
        ui_draw_info("  2. Insert an SD card (at least 1 GB free)");
        ui_draw_info("  3. Launch BootMii (from HBC or on boot)");
        ui_draw_info("  4. Navigate to 'Backup NAND'");
        ui_draw_info("  5. Wait for the backup to complete (~15 min)");
        ui_draw_info("  6. Keep nand.bin and keys.bin SAFE!");
        ui_printf("\n");
        ui_draw_warn("DO THIS BEFORE installing cIOS, themes, or WADs!");
    }

    /* Priiloader */
    ui_draw_section("Priiloader");

    if (s_has_priiloader) {
        ui_draw_ok("Priiloader app found on SD card");
        ui_draw_info("Priiloader protects against banner bricks and");
        ui_draw_info("bad System Menu updates. Make sure it's installed");
        ui_draw_info("to NAND (run it from HBC to install/update).");
    } else {
        ui_draw_warn("Priiloader not found on SD");
        ui_printf("\n");
        ui_draw_info("Priiloader is strongly recommended. It adds:");
        ui_draw_info("  - Banner brick protection");
        ui_draw_info("  - System Menu patches");
        ui_draw_info("  - Region-free loading");
        ui_draw_info("  - Auto-boot to HBC option");
        ui_printf("\n");
        ui_draw_info("Download from WiiBrew wiki and install via HBC.");
    }

    /* What to do next */
    ui_draw_section("What To Do Next");

    if (s_safety_score >= 80) {
        ui_draw_ok("Your brick protection is solid!");
        ui_draw_info("Keep your NAND backup + keys.bin stored safely");
        ui_draw_info("on your PC as well (not just SD card).");
    } else if (s_safety_score >= 40) {
        ui_draw_warn("Some improvements recommended:");
        if (!s_has_nand_backup)
            ui_draw_info("  -> Create a NAND backup with BootMii");
        if (!s_has_keys_bin && s_has_nand_backup)
            ui_draw_info("  -> Re-run BootMii backup to get keys.bin");
        if (!s_has_bootmii_ios)
            ui_draw_info("  -> Install BootMii via HackMii Installer");
        if (!s_has_priiloader)
            ui_draw_info("  -> Install Priiloader for extra protection");
    } else {
        ui_draw_err("Your Wii is at significant risk of unrecoverable brick!");
        ui_draw_info("  1. Install BootMii (priority #1)");
        ui_draw_info("  2. Create a NAND backup (priority #2)");
        ui_draw_info("  3. Install Priiloader (priority #3)");
        ui_draw_info("  4. Back up nand.bin + keys.bin to your PC");
        ui_printf("\n");
        ui_draw_info("Do NOT install cIOS, themes, or WADs until you");
        ui_draw_info("have completed at least steps 1 and 2.");
    }

    ui_printf("\n");
    ui_draw_ok("NAND backup check complete");
}

/*---------------------------------------------------------------------------*/
void get_nand_backup_report(char *buf, int bufsize) {
    snprintf(buf, bufsize,
        "=== NAND BACKUP & SAFETY CHECK ===\n"
        "Safety Score:        %d / 100\n"
        "BootMii IOS:         %s\n"
        "Boot2 Available:     %s\n"
        "NAND Backup:         %s\n"
        "Backup Size:         %.1f MB\n"
        "keys.bin:            %s\n"
        "Priiloader:          %s\n"
        "\n",
        s_safety_score,
        s_has_bootmii_ios ? "Installed" : "NOT FOUND",
        s_can_boot2 ? "Yes (Boot2 < v5)" : "No (Boot2 v5+)",
        s_has_nand_backup ? "Found" : "NOT FOUND",
        s_has_nand_backup ? (float)s_backup_size / (1024.0f * 1024.0f) : 0.0f,
        s_has_keys_bin ? "Found" : "NOT FOUND",
        s_has_priiloader ? "Found on SD" : "Not found"
    );
}
