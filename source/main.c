
#include <fat.h>
#include <gccore.h>
#include <ogc/system.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>

#include "controller_test.h"
#include "ios_check.h"
#include "nand_health.h"
#include "network_test.h"
#include "report.h"
#include "storage_test.h"
#include "system_info.h"
#include "ui_common.h"

#define MENU_ITEMS 8

static const char *menu_labels[MENU_ITEMS] = {
    "System Information",
    "NAND Health Check",
    "IOS Installation Scan",
    "Storage Speed Test (SD/USB)",
    "Controller Diagnostics",
    "Network Connectivity Test",
    "Generate Full Report to SD",
    "Exit to Homebrew Channel"
};

static const char *menu_descs[MENU_ITEMS] = {
    "Hardware revision, firmware, region, video mode, memory",
    "Scan NAND for space usage, file counts, and health score",
    "Audit installed IOS versions, detect stubs and cIOS",
    "Benchmark SD/USB read & write speeds, check filesystems",
    "Test GC controllers and Wii Remotes, detect stick drift",
    "Check WiFi module, IP config, internet connectivity",
    "Save a full diagnostic report as text file to SD card",
    "Return to the Homebrew Channel"
};

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;


static void init_video(void) {
    // this is boring, everybody does it the same way
    VIDEO_Init();
    rmode = VIDEO_GetPreferredMode(NULL);
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

    console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight,
                 rmode->fbWidth * VI_DISPLAY_PIX_SZ);

    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync(); // PAL needs two, don't why
}


static void draw_menu(int selected) {
    int i;

    ui_clear();
    ui_draw_banner();

    printf(UI_BCYAN "   DIAGNOSTIC MODULES\n" UI_RESET);
    printf(UI_WHITE "   -------------------\n\n" UI_RESET);

    for (i = 0; i < MENU_ITEMS; i++) {
        if (i == selected)
            printf(UI_BGREEN "   >> [%d] %s\n" UI_RESET, i + 1, menu_labels[i]);
        else
            printf(UI_WHITE "      [%d] %s\n" UI_RESET, i + 1, menu_labels[i]);
    }

    printf("\n" UI_YELLOW "   %s\n" UI_RESET, menu_descs[selected]);
    ui_draw_footer(NULL);
}


static void run_subscreen(const char *title, void (*func)(void)) {
    char spin_msg[80];
    ui_clear();
    ui_draw_banner();
    ui_draw_section(title);

    snprintf(spin_msg, sizeof(spin_msg), "Running %s...", title);
    ui_spin_start(spin_msg);
    ui_scroll_begin();
    func();
    ui_spin_stop();

    ui_scroll_view(title);
}


int main(int argc, char **argv) {
    int selected = 0;
    bool running = true;
    bool exit_to_hbc = false;

    init_video();
    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
    PAD_Init();
    fatInitDefault();

    while (running) {
        draw_menu(selected);

        while (1) {
            WPAD_ScanPads();
            PAD_ScanPads();

            u32 wpad = WPAD_ButtonsDown(0);
            u32 gpad = PAD_ButtonsDown(0);

            if ((wpad & WPAD_BUTTON_UP) || (gpad & PAD_BUTTON_UP)) {
                selected--;
                if (selected < 0)
                    selected = MENU_ITEMS - 1;
                break;
            }

            if ((wpad & WPAD_BUTTON_DOWN) || (gpad & PAD_BUTTON_DOWN)) {
                selected++;
                if (selected >= MENU_ITEMS)
                    selected = 0;
                break;
            }

            if ((wpad & WPAD_BUTTON_A) || (gpad & PAD_BUTTON_A)) {
                switch (selected) {
                    case 0: run_subscreen("System Information",    run_system_info);      break;
                    case 1: run_subscreen("NAND Health Check",     run_nand_health);      break;
                    case 2: run_subscreen("IOS Installation Scan", run_ios_check);        break;
                    case 3: run_subscreen("Storage Speed Test",    run_storage_test);     break;
                    case 4: run_subscreen("Controller Diagnostics",run_controller_test);  break;
                    case 5: run_subscreen("Network Connectivity",  run_network_test);     break;
                    case 6: run_subscreen("Generate Full Report",  run_report_generator); break;
                    case 7:
                        exit_to_hbc = true;
                        running = false;
                        break;
                }
                break;
            }

            if ((wpad & WPAD_BUTTON_HOME) || (gpad & PAD_BUTTON_START)) {
                running = false;
                break;
            }

            VIDEO_WaitVSync();
        }
    }

    ui_clear();
    printf(UI_BGREEN "\n  WiiMedic shutting down. Stay healthy!\n\n" UI_RESET);
    WPAD_Shutdown();

    if (exit_to_hbc) {
        // try HBC title IDs newest to oldest. LULZ is what HackMii installs these days.
        // OHBC is the vWii variant. JODI and HAXX are ancient but whatever,
        // fall back to system menu if none of them are installed.
        if (WII_LaunchTitle(0x000100014C554C5AULL) < 0)   // LULZ - modern
          if (WII_LaunchTitle(0x000100014F484243ULL) < 0)  // OHBC - vWii
            if (WII_LaunchTitle(0x000100014A4F4449ULL) < 0) // JODI - old
              if (WII_LaunchTitle(0x0001000148415858ULL) < 0) // HAXX - ancient
                SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
    } else {
        SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
    }

    return 0;
}
