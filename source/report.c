// report.c
// runs all the diagnostic modules and saves everything to a text file on SD or USB.
// if a module has already been run from the main menu, we use the cached results
// instead of running it again. saves time and avoids hammering the NAND twice.

#include <fat.h>
#include <gccore.h>
#include <malloc.h>
#include <network.h>
#include <ogc/lwp.h>
#include <ogc/lwp_watchdog.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wiiuse/wpad.h>

#include "controller_test.h"
#include "ios_check.h"
#include "nand_health.h"
#include "network_test.h"
#include "report.h"
#include "storage_test.h"
#include "system_info.h"
#include "ui_common.h"

#define REPORT_PATH_SD  "sd:/WiiMedic_Report.txt"
#define REPORT_PATH_USB "usb:/WiiMedic_Report.txt"


static void report_write(FILE *fp, const char *fmt, ...) {
    if (!fp) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);
}


// check if a report already exists at this path. returns file size or -1.
static long check_existing(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);
    return sz;
}


// find the next available filename like WiiMedic_Report_2.txt, _3.txt, etc.
// gives up at 99 and just overwrites that one.
static void next_available_filename(const char *dir, char *out, int outsize) {
    int n;
    for (n = 2; n <= 99; n++) {
        snprintf(out, outsize, "%s/WiiMedic_Report_%d.txt", dir, n);
        FILE *f = fopen(out, "r");
        if (!f) return;  // this name is free
        fclose(f);
    }
    snprintf(out, outsize, "%s/WiiMedic_Report_99.txt", dir);
}


// shows a little menu when we find an existing report.
// returns 0 = replace, 1 = keep both, 2 = cancel
static int ask_what_to_do(const char *path, long size) {
    int sel = 0;
    char szstr[64];
    snprintf(szstr, sizeof(szstr), "%.1f KB", (float)size / 1024.0f);

    while (1) {
        u32 wpad, gpad;

        printf("\x1b[2J\x1b[0;0H");
        printf(UI_BGREEN " [+] WiiMedic" UI_RESET " " UI_CYAN "v" WIIMEDIC_VERSION UI_RESET "\n");
        printf(UI_WHITE " -----------------------------------------------------------\n" UI_RESET);

        printf("\n" UI_BYELLOW "   Found an existing report!\n\n" UI_RESET);
        printf("   " UI_CYAN "File" UI_RESET " ......... " UI_BWHITE "%s\n" UI_RESET, path);
        printf("   " UI_CYAN "Size" UI_RESET " ......... " UI_BWHITE "%s\n" UI_RESET, szstr);
        printf("\n" UI_WHITE "   What do you want to do?\n\n" UI_RESET);

        printf(sel == 0 ? UI_BGREEN "   >> [1] Overwrite it\n"          UI_RESET
                        : UI_WHITE  "      [1] Overwrite it\n"           UI_RESET);
        printf(sel == 1 ? UI_BGREEN "   >> [2] Keep it, save new file alongside\n" UI_RESET
                        : UI_WHITE  "      [2] Keep it, save new file alongside\n" UI_RESET);
        printf(sel == 2 ? UI_BGREEN "   >> [3] Cancel\n"                UI_RESET
                        : UI_WHITE  "      [3] Cancel\n"                 UI_RESET);

        printf("\n" UI_WHITE " -----------------------------------------------------------\n" UI_RESET);
        printf(UI_WHITE " [UP/DOWN] Choose   [A] Confirm   [B] Cancel\n" UI_RESET);

        while (1) {
            bool brk = false;
            WPAD_ScanPads();
            PAD_ScanPads();
            wpad = WPAD_ButtonsDown(0);
            gpad = PAD_ButtonsDown(0);

            if ((wpad & WPAD_BUTTON_UP) || (gpad & PAD_BUTTON_UP)) {
                if (--sel < 0) sel = 2;
                brk = true;
            }
            if ((wpad & WPAD_BUTTON_DOWN) || (gpad & PAD_BUTTON_DOWN)) {
                if (++sel > 2) sel = 0;
                brk = true;
            }
            if ((wpad & WPAD_BUTTON_A) || (gpad & PAD_BUTTON_A))
                return sel;
            if ((wpad & WPAD_BUTTON_B) || (gpad & PAD_BUTTON_B))
                return 2;

            if (brk) break;
            VIDEO_WaitVSync();
        }
    }
}


void run_report_generator(void) {
    // static so we don't blow the stack - 8KB on the stack is a bad idea
    static char section[8192];
    // save_path as a fixed buffer so we never have a dangling pointer
    static char save_path[256];
    char buf[128];
    FILE *fp = NULL;

    ui_draw_info("This runs all diagnostic modules and saves results to SD/USB.");
    ui_draw_info("If you already ran a module, we'll use those results.");
    ui_printf("\n");

    save_path[0] = '\0';

    long existing_sd  = check_existing(REPORT_PATH_SD);
    long existing_usb = check_existing(REPORT_PATH_USB);
    long existing_sz  = -1;
    // which base dir the existing report lives on (for numbering new files)
    const char *base_dir = NULL;
    // path of the existing report (points into REPORT_PATH_SD/USB literals)
    const char *existing_path = NULL;

    if (existing_sd >= 0) {
        existing_sz   = existing_sd;
        existing_path = REPORT_PATH_SD;
        base_dir      = "sd:";
    } else if (existing_usb >= 0) {
        existing_sz   = existing_usb;
        existing_path = REPORT_PATH_USB;
        base_dir      = "usb:";
    }

    if (existing_sz >= 0) {
        // ask_what_to_do uses raw printf so we must NOT be in scroll mode.
        // ui_scroll_view hasn't been entered yet at this point so we're fine,
        // but call VIDEO_WaitVSync first to make sure the last frame is flushed.
        VIDEO_WaitVSync();

        int action = ask_what_to_do(existing_path, existing_sz);
        if (action == 2) {
            ui_draw_info("Cancelled.");
            return;
        } else if (action == 1) {
            // keep the old one, find a new numbered name
            next_available_filename(base_dir, save_path, sizeof(save_path));
        } else {
            // overwrite
            strncpy(save_path, existing_path, sizeof(save_path) - 1);
            save_path[sizeof(save_path) - 1] = '\0';
        }
    } else {
        // no existing report - try SD first, then USB
        fp = fopen(REPORT_PATH_SD, "w");
        if (fp) {
            fclose(fp);
            strncpy(save_path, REPORT_PATH_SD, sizeof(save_path) - 1);
        } else {
            fp = fopen(REPORT_PATH_USB, "w");
            if (fp) {
                fclose(fp);
                strncpy(save_path, REPORT_PATH_USB, sizeof(save_path) - 1);
            }
        }
        save_path[sizeof(save_path) - 1] = '\0';
    }

    if (save_path[0] == '\0') {
        ui_draw_err("No writable storage found!");
        ui_draw_warn("Insert an SD card or USB drive and try again.");
        return;
    }

    // clear the dialog off screen and show something immediately so the
    // user knows we're working - on real hardware the first module takes
    // a second or two to init and it looks totally frozen without this
    printf("\x1b[2J\x1b[0;0H");
    printf(UI_BGREEN " [+] WiiMedic" UI_RESET " " UI_CYAN "v" WIIMEDIC_VERSION UI_RESET "\n");
    printf(UI_WHITE " -----------------------------------------------------------\n" UI_RESET);
    printf("\n" UI_BYELLOW "   Preparing report, please wait...\n" UI_RESET);
    printf("\n" UI_WHITE "   This may take a minute on real hardware.\n" UI_RESET);
    VIDEO_WaitVSync();
    VIDEO_WaitVSync();

    fp = fopen(save_path, "w");
    if (!fp) {
        ui_draw_err("Failed to open file for writing!");
        ui_draw_warn("Check that the card isn't write-protected.");
        return;
    }

    report_write(fp,
        "==========================================================\n"
        "     WiiMedic Diagnostic Report v" WIIMEDIC_VERSION "\n"
        "==========================================================\n\n"
        "Generated by WiiMedic - Wii System Diagnostic & Health Monitor\n"
        "Post this file when asking for help on forums or Reddit.\n"
        "----------------------------------------------------------\n\n");

    // [1/6] system info
    // does ISFS reads which can stall for a couple seconds, hence the spinner
    ui_printf(UI_BCYAN "   [1/6]" UI_WHITE " System information...\n" UI_RESET);
    ui_spin_start("Gathering system info...");
    memset(section, 0, sizeof(section));
    get_system_info_report(section, sizeof(section));
    ui_spin_stop();
    report_write(fp, "%s", section);
    ui_draw_ok("Done.");

    // [2/6] NAND health
    ui_printf(UI_BCYAN "   [2/6]" UI_WHITE " NAND health...\n" UI_RESET);
    if (!has_nand_health_run()) {
        run_nand_health();
    } else {
        ui_printf("   " UI_WHITE "(using cached results)\n" UI_RESET);
    }
    memset(section, 0, sizeof(section));
    get_nand_health_report(section, sizeof(section));
    report_write(fp, "%s", section);
    ui_draw_ok("Done.");

    // [3/6] IOS scan
    ui_printf(UI_BCYAN "   [3/6]" UI_WHITE " IOS installations...\n" UI_RESET);
    memset(section, 0, sizeof(section));
    get_ios_check_report(section, sizeof(section));
    if (strlen(section) > 0) {
        report_write(fp, "%s", section);
    } else {
        report_write(fp,
            "=== IOS INSTALLATION SCAN ===\n"
            "Not run yet. Go to the main menu and run IOS Scan first for full data.\n\n");
    }
    ui_draw_ok("Done.");

    // [4/6] storage
    ui_printf(UI_BCYAN "   [4/6]" UI_WHITE " Storage devices...\n" UI_RESET);
    memset(section, 0, sizeof(section));
    get_storage_test_report(section, sizeof(section));
    if (strlen(section) > 0) {
        report_write(fp, "%s", section);
    } else {
        report_write(fp,
            "=== STORAGE TEST ===\n"
            "Not run yet. Run Storage Test from main menu for full data.\n\n");
    }
    ui_draw_ok("Done.");

    // [5/6] controllers
    // has a ~30-frame BT warmup that looks frozen without a spinner
    ui_printf(UI_BCYAN "   [5/6]" UI_WHITE " Controllers...\n" UI_RESET);
    ui_spin_start("Scanning controllers...");
    scan_controllers_quick();
    ui_spin_stop();
    memset(section, 0, sizeof(section));
    get_controller_test_report(section, sizeof(section));
    report_write(fp, "%s", section);
    ui_draw_ok("Done.");

    // [6/6] network
    // if already tested we skip re-running it - network test is slow
    if (has_network_test_run()) {
        ui_printf(UI_BCYAN "   [6/6]" UI_WHITE " Network (cached)...\n" UI_RESET);
    } else {
        ui_printf(UI_BCYAN "   [6/6]" UI_WHITE " Network...\n" UI_RESET);
        // let IOS breathe after the NAND/IOS reads before hitting the WiFi stack
        net_deinit();
        {
            int i;
            for (i = 0; i < 60; i++) VIDEO_WaitVSync();
        }
        run_network_test();
    }
    memset(section, 0, sizeof(section));
    get_network_test_report(section, sizeof(section));
    if (strlen(section) > 0) {
        report_write(fp, "%s", section);
    } else {
        report_write(fp,
            "=== NETWORK TEST ===\n"
            "Not run yet. Run Network Test from main menu for full data.\n\n");
    }
    ui_draw_ok("Done.");

    report_write(fp,
        "----------------------------------------------------------\n"
        "END OF WIIMEDIC DIAGNOSTIC REPORT\n"
        "For best results, run each module individually first,\n"
        "then regenerate this report to capture everything.\n"
        "----------------------------------------------------------\n");

    long file_size = ftell(fp);
    fclose(fp);

    ui_printf("\n");
    ui_draw_ok("Report saved!");

    char pathmsg[192];
    snprintf(pathmsg, sizeof(pathmsg), "Saved to: %s", save_path);
    ui_draw_ok(pathmsg);

    snprintf(buf, sizeof(buf), "File size: %ld bytes", file_size);
    ui_draw_ok(buf);

    if (existing_sz >= 0) {
        if (strcmp(save_path, REPORT_PATH_SD) == 0 ||
            strcmp(save_path, REPORT_PATH_USB) == 0)
            ui_draw_info("Old report was overwritten.");
        else {
            ui_draw_info("Old report was kept.");
            snprintf(pathmsg, sizeof(pathmsg), "New report: %s", save_path);
            ui_draw_info(pathmsg);
        }
    }

    ui_printf("\n");
    ui_draw_info("Copy the .txt file to your PC and paste it when asking for help.");
}
