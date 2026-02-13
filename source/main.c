/*
 * WiiMedic - main.c
 * Application entry point and polished menu system
 * All UI uses ASCII-safe characters only
 */

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

/* Menu configuration */
#define MENU_ITEMS 8

static const char *menu_labels[MENU_ITEMS] = {
    "System Information",         "NAND Health Check",
    "IOS Installation Scan",      "Storage Speed Test (SD/USB)",
    "Controller Diagnostics",     "Network Connectivity Test",
    "Generate Full Report to SD", "Exit to Homebrew Channel"};

static const char *menu_descs[MENU_ITEMS] = {
    "Hardware revision, firmware, region, video mode, memory",
    "Scan NAND for space usage, file counts, and health score",
    "Audit installed IOS versions, detect stubs and cIOS",
    "Benchmark SD/USB read & write speeds, check filesystems",
    "Test GC controllers and Wii Remotes, detect stick drift",
    "Check WiFi module, IP config, internet connectivity",
    "Save a full diagnostic report as text file to SD card",
    "Return to the Homebrew Channel"};

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

/* Konami code: UP UP DOWN DOWN LEFT RIGHT LEFT RIGHT B A */
#define KONAMI_LEN 10
static const u32 konami_wpad[KONAMI_LEN] = {
    WPAD_BUTTON_UP,   WPAD_BUTTON_UP,    WPAD_BUTTON_DOWN, WPAD_BUTTON_DOWN,
    WPAD_BUTTON_LEFT, WPAD_BUTTON_RIGHT, WPAD_BUTTON_LEFT, WPAD_BUTTON_RIGHT,
    WPAD_BUTTON_B,    WPAD_BUTTON_A};
static const u32 konami_gpad[KONAMI_LEN] = {
    PAD_BUTTON_UP,   PAD_BUTTON_UP,    PAD_BUTTON_DOWN, PAD_BUTTON_DOWN,
    PAD_BUTTON_LEFT, PAD_BUTTON_RIGHT, PAD_BUTTON_LEFT, PAD_BUTTON_RIGHT,
    PAD_BUTTON_B,    PAD_BUTTON_A};
static int konami_pos = 0;

/*---------------------------------------------------------------------------*/
static void init_video(void) {
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
    VIDEO_WaitVSync();
}

/*---------------------------------------------------------------------------*/
static void draw_menu(int selected) {
  int i;

  ui_clear();
  ui_draw_banner();

  printf(UI_BCYAN "   DIAGNOSTIC MODULES\n" UI_RESET);
  printf(UI_WHITE "   -------------------\n\n" UI_RESET);

  for (i = 0; i < MENU_ITEMS; i++) {
    if (i == selected) {
      printf(UI_BGREEN "   >> [%d] %s\n" UI_RESET, i + 1, menu_labels[i]);
    } else {
      printf(UI_WHITE "      [%d] %s\n" UI_RESET, i + 1, menu_labels[i]);
    }
  }

  printf("\n" UI_YELLOW "   %s\n" UI_RESET, menu_descs[selected]);

  ui_draw_footer(NULL);
}

/*---------------------------------------------------------------------------*/
static void show_easter_egg(void) {
  int i;

  ui_clear();
  printf("\n\n");
  printf(UI_BGREEN "         ___________________________________\n");
  printf("        |       ____                        |\n");
  printf("        |      |    \\ _ __                  |\n");
  printf("        |      |  D  | '__| __      _  _    |\n");
  printf("        |      |  D  | |   /  \\   / \\/ \\   |\n");
  printf("        |      |  D  | |  | () |  | |/| |   |\n");
  printf("        |      |____/|_|   \\__/\\_ |_|  |_|   |\n");
  printf("        |                                    |\n");
  printf("        |   DIAGNOSIS: Your Wii is AWESOME   |\n");
  printf("        |___________________________________|\n" UI_RESET);
  printf("\n");
  printf(UI_BCYAN "          Rx: Keep playing games daily.\n" UI_RESET);
  printf(UI_BYELLOW "          Side effects may include: fun.\n\n" UI_RESET);
  printf(UI_WHITE "               - Dr. Wii, M.D. -\n\n" UI_RESET);
  printf(UI_MAGENTA "          You found the secret! :)\n" UI_RESET);
  printf("\n");
  printf(UI_WHITE "         Returning in " UI_RESET);

  for (i = 3; i > 0; i--) {
    printf(UI_BGREEN "%d..." UI_RESET, i);
    VIDEO_WaitVSync();
    /* ~1 second delay using vsync (approx 60 vsyncs) */
    {
      int v;
      for (v = 0; v < 60; v++)
        VIDEO_WaitVSync();
    }
  }
}

/*---------------------------------------------------------------------------*/
static void run_subscreen(const char *title, void (*func)(void)) {
  ui_clear();
  ui_draw_banner();
  ui_draw_section(title);
  printf(UI_WHITE "   Processing, please wait...\n" UI_RESET);

  ui_scroll_begin();
  func();
  ui_scroll_view(title);
}

/*---------------------------------------------------------------------------*/
int main(int argc, char **argv) {
  int selected = 0;
  bool running = true;
  konami_pos = 0;

  /* Initialize subsystems */
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

      /* --- Konami code tracking (runs before navigation) --- */
      if (wpad || gpad) {
        bool matched = false;
        if (wpad & konami_wpad[konami_pos])
          matched = true;
        else if (gpad & konami_gpad[konami_pos])
          matched = true;

        if (matched) {
          konami_pos++;
          if (konami_pos >= KONAMI_LEN) {
            konami_pos = 0;
            show_easter_egg();
            break; /* redraw menu */
          }
        } else {
          konami_pos = 0;
          /* If they pressed the first button of the sequence, start tracking */
          if ((wpad & konami_wpad[0]) || (gpad & konami_gpad[0]))
            konami_pos = 1;
        }
      }

      /* Navigate up */
      if ((wpad & WPAD_BUTTON_UP) || (gpad & PAD_BUTTON_UP)) {
        selected--;
        if (selected < 0)
          selected = MENU_ITEMS - 1;
        break;
      }

      /* Navigate down */
      if ((wpad & WPAD_BUTTON_DOWN) || (gpad & PAD_BUTTON_DOWN)) {
        selected++;
        if (selected >= MENU_ITEMS)
          selected = 0;
        break;
      }

      /* Select item */
      if ((wpad & WPAD_BUTTON_A) || (gpad & PAD_BUTTON_A)) {
        switch (selected) {
        case 0:
          run_subscreen("System Information", run_system_info);
          break;
        case 1:
          run_subscreen("NAND Health Check", run_nand_health);
          break;
        case 2:
          run_subscreen("IOS Installation Scan", run_ios_check);
          break;
        case 3:
          run_subscreen("Storage Speed Test", run_storage_test);
          break;
        case 4:
          run_subscreen("Controller Diagnostics", run_controller_test);
          break;
        case 5:
          run_subscreen("Network Connectivity", run_network_test);
          break;
        case 6:
          run_subscreen("Generate Full Report", run_report_generator);
          break;
        case 7:
          running = false;
          break;
        }
        break;
      }

      /* Exit via HOME */
      if ((wpad & WPAD_BUTTON_HOME) || (gpad & PAD_BUTTON_START)) {
        running = false;
        break;
      }

      VIDEO_WaitVSync();
    }
  }

  /* Cleanup */
  ui_clear();
  printf(UI_BGREEN "\n  WiiMedic shutting down. Stay healthy!\n\n" UI_RESET);
  WPAD_Shutdown();
  WII_LaunchTitle(0x0001000148415858ULL); // HBC title ID for HAXX
  return 0;
}
