// controller_test.c
// reads the current state of GC controllers and Wii Remotes, checks for drift.
// this is a snapshot test, not a live viewer - tell the user to hold buttons
// before running it if they want to see button presses register.

#include <gccore.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>

#include "controller_test.h"
#include "ui_common.h"

static int s_gc_detected  = 0;
static int s_wm_detected  = 0;


// maps raw battery level byte to bars (0-4).
// the thresholds come from actual measured values on real hardware.
// 0x55 and above = full, it drops from there. 0x03 is basically dead.
static unsigned battery_raw_to_bars(u8 raw) {
    if (raw >= 0x55) return 4;
    if (raw >= 0x44) return 3;
    if (raw >= 0x33) return 2;
    if (raw >= 0x03) return 1;
    return 0; // lol
}


static void test_gc_controllers(void) {
    int port;
    char buf[64];

    ui_draw_section("GameCube Controller Ports");
    s_gc_detected = 0;

    // PAD_ScanPads returns a bitmask of connected ports
    u32 connected = PAD_ScanPads();

    for (port = 0; port < 4; port++) {
        if (!(connected & (1 << port))) {
            snprintf(buf, sizeof(buf), "Port %d: Nothing here", port + 1);
            ui_printf("   " UI_WHITE "%s\n" UI_RESET, buf);
            continue;
        }

        s16 sx = PAD_StickX(port),    sy = PAD_StickY(port);
        s16 cx = PAD_SubStickX(port), cy = PAD_SubStickY(port);
        u16 btns = PAD_ButtonsHeld(port);
        u8  tl = PAD_TriggerL(port),  tr = PAD_TriggerR(port);

        s_gc_detected++;

        snprintf(buf, sizeof(buf), "Port %d: CONNECTED", port + 1);
        ui_draw_ok(buf);

        snprintf(buf, sizeof(buf), "X=%+4d  Y=%+4d", sx, sy);
        ui_draw_kv("  Main Stick", buf);

        snprintf(buf, sizeof(buf), "X=%+4d  Y=%+4d", cx, cy);
        ui_draw_kv("  C-Stick", buf);

        snprintf(buf, sizeof(buf), "L=%3d  R=%3d", tl, tr);
        ui_draw_kv("  Triggers", buf);

        ui_printf("   " UI_CYAN "  Buttons " UI_RESET "................. ");
        if (btns & PAD_BUTTON_A)     ui_printf(UI_BGREEN "A "     UI_RESET);
        if (btns & PAD_BUTTON_B)     ui_printf(UI_BGREEN "B "     UI_RESET);
        if (btns & PAD_BUTTON_X)     ui_printf(UI_BGREEN "X "     UI_RESET);
        if (btns & PAD_BUTTON_Y)     ui_printf(UI_BGREEN "Y "     UI_RESET);
        if (btns & PAD_TRIGGER_Z)    ui_printf(UI_BGREEN "Z "     UI_RESET);
        if (btns & PAD_BUTTON_START) ui_printf(UI_BGREEN "START " UI_RESET);
        if (btns == 0)               ui_printf(UI_WHITE "(none)" UI_RESET);
        ui_printf("\n");

        // drift check - 8 units from center with no buttons held is suspicious.
        // real drift is usually way more than this but it's a starting point.
        {
            float d = sqrtf((float)(sx*sx + sy*sy));
            if (d > 8.0f && btns == 0) {
                snprintf(buf, sizeof(buf), "Main stick may be drifting (dist from center: %.0f)", d);
                ui_draw_warn(buf);
            }
        }
        {
            float d = sqrtf((float)(cx*cx + cy*cy));
            if (d > 8.0f && btns == 0) {
                snprintf(buf, sizeof(buf), "C-Stick may be drifting (dist from center: %.0f)", d);
                ui_draw_warn(buf);
            }
        }

        ui_printf("\n");
    }
}


static void test_wiimotes(void) {
    int chan;
    char buf[128];

    ui_draw_section("Wii Remote / Extensions");
    s_wm_detected = 0;

    // the Bluetooth stack needs a few frames to settle after WPAD_Init.
    // one scan is never enough and the results are garbage. 30 frames
    // works consistently. annoying but that's how it is.
    {
        int i;
        for (i = 0; i < 30; i++) {
            WPAD_ScanPads();
            VIDEO_WaitVSync();
        }
    }

    for (chan = 0; chan < 4; chan++) {
        u32 type;
        s32 ret = WPAD_Probe(chan, &type);

        if (ret == WPAD_ERR_NONE) {
            s_wm_detected++;

            const char *ext;
            switch (type) {
                case WPAD_EXP_NONE:       ext = "No Extension";          break;
                case WPAD_EXP_NUNCHUK:    ext = "Nunchuk";               break;
                case WPAD_EXP_CLASSIC:    ext = "Classic Controller";    break;
                case WPAD_EXP_GUITARHERO3:ext = "Guitar Hero Controller";break;
                case WPAD_EXP_WIIBOARD:   ext = "Balance Board";         break;
                default:                  ext = "Unknown Extension";     break;
            }

            snprintf(buf, sizeof(buf), "Wii Remote %d: CONNECTED", chan + 1);
            ui_draw_ok(buf);
            ui_draw_kv("  Extension", ext);

            WPADData *wd = WPAD_Data(chan);
            if (wd) {
                u32 btns = WPAD_ButtonsHeld(chan);

                ui_printf("   " UI_CYAN "  Buttons " UI_RESET "................. ");
                if (btns & WPAD_BUTTON_A)     ui_printf(UI_BGREEN "A "    UI_RESET);
                if (btns & WPAD_BUTTON_B)     ui_printf(UI_BGREEN "B "    UI_RESET);
                if (btns & WPAD_BUTTON_1)     ui_printf(UI_BGREEN "1 "    UI_RESET);
                if (btns & WPAD_BUTTON_2)     ui_printf(UI_BGREEN "2 "    UI_RESET);
                if (btns & WPAD_BUTTON_PLUS)  ui_printf(UI_BGREEN "+ "    UI_RESET);
                if (btns & WPAD_BUTTON_MINUS) ui_printf(UI_BGREEN "- "    UI_RESET);
                if (btns & WPAD_BUTTON_HOME)  ui_printf(UI_BGREEN "HOME " UI_RESET);
                if (btns == 0)                ui_printf(UI_WHITE "(none)" UI_RESET);
                ui_printf("\n");

                // battery display. show bars and color-code it.
                {
                    unsigned bars = battery_raw_to_bars(wd->battery_level);
                    const char *bcolor = (bars >= 3) ? UI_BGREEN :
                                         (bars >= 2) ? UI_BYELLOW : UI_BRED;
                    snprintf(buf, sizeof(buf), "%u / 4 bars", bars);
                    ui_draw_kv_color("  Battery", bcolor, buf);
                }

                if (wd->ir.valid)
                    ui_draw_kv_color("  IR Sensor", UI_BGREEN, "Working");
                else
                    ui_draw_kv("  IR Sensor", "Not pointing at sensor bar");

                // nunchuk stick - check for drift same way as GC sticks
                if (type == WPAD_EXP_NUNCHUK) {
                    s8 nx = wd->exp.nunchuk.js.pos.x - wd->exp.nunchuk.js.center.x;
                    s8 ny = wd->exp.nunchuk.js.pos.y - wd->exp.nunchuk.js.center.y;

                    snprintf(buf, sizeof(buf), "X=%+4d  Y=%+4d", nx, ny);
                    ui_draw_kv("  Nunchuk Stick", buf);

                    float d = sqrtf((float)(nx*nx + ny*ny));
                    if (d > 15.0f) {
                        snprintf(buf, sizeof(buf), "Nunchuk stick might be drifting (dist: %.0f)", d);
                        ui_draw_warn(buf);
                    }
                }
            }

            ui_printf("\n");

        } else if (ret == WPAD_ERR_NOT_READY) {
            snprintf(buf, sizeof(buf), "Wii Remote %d: Still connecting...", chan + 1);
            ui_printf("   " UI_BYELLOW "%s\n" UI_RESET, buf);
        } else {
            snprintf(buf, sizeof(buf), "Wii Remote %d: Not connected", chan + 1);
            ui_printf("   " UI_WHITE "%s\n" UI_RESET, buf);
        }
    }
}


void run_controller_test(void) {
    char buf[64];

    ui_draw_info("This is a one-shot snapshot. Hold any buttons you want to check");
    ui_draw_info("BEFORE running this, otherwise they won't register.");
    ui_printf("\n");

    PAD_ScanPads();
    WPAD_ScanPads();

    test_gc_controllers();
    test_wiimotes();

    ui_draw_section("Summary");

    snprintf(buf, sizeof(buf), "%d / 4", s_gc_detected);
    ui_draw_kv("GameCube Ports Active", buf);

    snprintf(buf, sizeof(buf), "%d / 4", s_wm_detected);
    ui_draw_kv("Wii Remotes Connected", buf);

    if (s_gc_detected == 0 && s_wm_detected <= 1)
        ui_draw_info("Connect some controllers and re-run to test them");

    ui_printf("\n");
    ui_draw_ok("Controller check done");
}


// quick version used by the report generator - just needs counts, not UI output
void scan_controllers_quick(void) {
    int i;

    s_gc_detected = 0;
    {
        u32 mask = PAD_ScanPads();
        for (i = 0; i < 4; i++) {
            if (mask & (1 << i)) s_gc_detected++;
        }
    }

    // still need the BT warmup, no way around it
    s_wm_detected = 0;
    for (i = 0; i < 30; i++) {
        WPAD_ScanPads();
        VIDEO_WaitVSync();
    }
    for (i = 0; i < 4; i++) {
        u32 type;
        if (WPAD_Probe(i, &type) == WPAD_ERR_NONE)
            s_wm_detected++;
    }
}


void get_controller_test_report(char *buf, int bufsize) {
    snprintf(buf, bufsize,
             "=== CONTROLLER DIAGNOSTICS ===\n"
             "GameCube Ports Active: %d / 4\n"
             "Wii Remotes Connected: %d / 4\n"
             "\n",
             s_gc_detected, s_wm_detected);
}
