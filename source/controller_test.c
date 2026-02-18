/*
 * WiiMedic - controller_test.c
 * Tests GameCube controller ports and Wii Remote connections
 */

#include <gccore.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>

#include "controller_test.h"
#include "ui_common.h"

static int s_gc_ports_detected = 0;
static int s_wiimotes_detected = 0;

/* Battery thresholds provided by dkosmari */
static unsigned get_battery_bars(u8 raw) {
  if (raw >= 0x55)
    return 4;
  if (raw >= 0x44)
    return 3;
  if (raw >= 0x33)
    return 2;
  if (raw >= 0x03)
    return 1;
  return 0;
}

/*---------------------------------------------------------------------------*/
static void test_gc_controllers(void) {
  int port;
  char buf[64];

  ui_draw_section("GameCube Controller Ports");

  s_gc_ports_detected = 0;
  PAD_ScanPads();

  for (port = 0; port < 4; port++) {
    s16 stickX = PAD_StickX(port);
    s16 stickY = PAD_StickY(port);
    s16 cstickX = PAD_SubStickX(port);
    s16 cstickY = PAD_SubStickY(port);
    u16 btns = PAD_ButtonsHeld(port);
    u8 trigL = PAD_TriggerL(port);
    u8 trigR = PAD_TriggerR(port);

    bool connected =
        (stickX || stickY || cstickX || cstickY || btns || trigL || trigR);

    if (connected) {
      s_gc_ports_detected++;

      snprintf(buf, sizeof(buf), "Port %d: CONNECTED", port + 1);
      ui_draw_ok(buf);

      snprintf(buf, sizeof(buf), "X=%+4d  Y=%+4d", stickX, stickY);
      ui_draw_kv("  Main Stick", buf);

      snprintf(buf, sizeof(buf), "X=%+4d  Y=%+4d", cstickX, cstickY);
      ui_draw_kv("  C-Stick", buf);

      snprintf(buf, sizeof(buf), "L=%3d  R=%3d", trigL, trigR);
      ui_draw_kv("  Triggers", buf);

      /* Button display */
      ui_printf("   " UI_CYAN "  Buttons " UI_RESET "................. ");
      if (btns & PAD_BUTTON_A)
        ui_printf(UI_BGREEN "A " UI_RESET);
      if (btns & PAD_BUTTON_B)
        ui_printf(UI_BGREEN "B " UI_RESET);
      if (btns & PAD_BUTTON_X)
        ui_printf(UI_BGREEN "X " UI_RESET);
      if (btns & PAD_BUTTON_Y)
        ui_printf(UI_BGREEN "Y " UI_RESET);
      if (btns & PAD_TRIGGER_Z)
        ui_printf(UI_BGREEN "Z " UI_RESET);
      if (btns & PAD_BUTTON_START)
        ui_printf(UI_BGREEN "START " UI_RESET);
      if (btns == 0)
        ui_printf(UI_WHITE "(none held)" UI_RESET);
      ui_printf("\n");

      /* Drift check */
      {
        float stick_dist = sqrtf((float)(stickX * stickX + stickY * stickY));
        if (stick_dist > 15.0f && btns == 0) {
          snprintf(buf, sizeof(buf),
                   "Main stick drift detected (distance: %.0f)", stick_dist);
          ui_draw_warn(buf);
        }
      }
      {
        float cstick_dist =
            sqrtf((float)(cstickX * cstickX + cstickY * cstickY));
        if (cstick_dist > 15.0f && btns == 0) {
          snprintf(buf, sizeof(buf), "C-Stick drift detected (distance: %.0f)",
                   cstick_dist);
          ui_draw_warn(buf);
        }
      }

      ui_printf("\n");
    } else {
      snprintf(buf, sizeof(buf), "Port %d: No controller detected", port + 1);
      ui_printf("   " UI_WHITE "%s\n" UI_RESET, buf);
    }
  }
}

/*---------------------------------------------------------------------------*/
static void test_wiimotes(void) {
  int chan;
  char buf[128];

  ui_draw_section("Wii Remote / Extensions");

  s_wiimotes_detected = 0;

  /* Give the Bluetooth stack several frames to update connection state.
     A single WPAD_ScanPads() is often not enough for WPAD_Probe()
     to return accurate results. */
  {
    int warmup;
    for (warmup = 0; warmup < 30; warmup++) {
      WPAD_ScanPads();
      VIDEO_WaitVSync();
    }
  }

  for (chan = 0; chan < 4; chan++) {
    u32 type;
    s32 ret = WPAD_Probe(chan, &type);

    if (ret == WPAD_ERR_NONE) {
      s_wiimotes_detected++;

      const char *ext_name;
      switch (type) {
      case WPAD_EXP_NONE:
        ext_name = "No Extension";
        break;
      case WPAD_EXP_NUNCHUK:
        ext_name = "Nunchuk";
        break;
      case WPAD_EXP_CLASSIC:
        ext_name = "Classic Controller";
        break;
      case WPAD_EXP_GUITARHERO3:
        ext_name = "Guitar Hero Controller";
        break;
      case WPAD_EXP_WIIBOARD:
        ext_name = "Balance Board";
        break;
      default:
        ext_name = "Unknown Extension";
        break;
      }

      snprintf(buf, sizeof(buf), "Wii Remote %d: CONNECTED", chan + 1);
      ui_draw_ok(buf);
      ui_draw_kv("  Extension", ext_name);

      /* Buttons */
      {
        WPADData *wdata = WPAD_Data(chan);
        if (wdata) {
          u32 btns = WPAD_ButtonsHeld(chan);

          ui_printf("   " UI_CYAN "  Buttons " UI_RESET "................. ");
          if (btns & WPAD_BUTTON_A)
            ui_printf(UI_BGREEN "A " UI_RESET);
          if (btns & WPAD_BUTTON_B)
            ui_printf(UI_BGREEN "B " UI_RESET);
          if (btns & WPAD_BUTTON_1)
            ui_printf(UI_BGREEN "1 " UI_RESET);
          if (btns & WPAD_BUTTON_2)
            ui_printf(UI_BGREEN "2 " UI_RESET);
          if (btns & WPAD_BUTTON_PLUS)
            ui_printf(UI_BGREEN "+ " UI_RESET);
          if (btns & WPAD_BUTTON_MINUS)
            ui_printf(UI_BGREEN "- " UI_RESET);
          if (btns & WPAD_BUTTON_HOME)
            ui_printf(UI_BGREEN "HOME " UI_RESET);
          if (btns == 0)
            ui_printf(UI_WHITE "(none held)" UI_RESET);
          ui_printf("\n");

          /* Battery */
          {
            u8 raw = wdata->battery_level;
            unsigned bars = get_battery_bars(raw);

            const char *batt_color;
            batt_color = (bars >= 3)   ? UI_BGREEN
                         : (bars >= 2) ? UI_BYELLOW
                                       : UI_BRED;

            snprintf(buf, sizeof(buf), "%u / 4 bars", bars);
            ui_draw_kv_color("  Battery", batt_color, buf);
          }

          /* IR sensor */
          if (wdata->ir.valid) {
            ui_draw_kv_color("  IR Sensor", UI_BGREEN,
                             "Working (pointing at sensor bar)");
          } else {
            ui_draw_kv("  IR Sensor", "Not pointing at sensor bar");
          }

          /* Nunchuk stick */
          if (type == WPAD_EXP_NUNCHUK) {
            s8 nun_x =
                wdata->exp.nunchuk.js.pos.x - wdata->exp.nunchuk.js.center.x;
            s8 nun_y =
                wdata->exp.nunchuk.js.pos.y - wdata->exp.nunchuk.js.center.y;
            float nun_dist;

            snprintf(buf, sizeof(buf), "X=%+4d  Y=%+4d", nun_x, nun_y);
            ui_draw_kv("  Nunchuk Stick", buf);

            nun_dist = sqrtf((float)(nun_x * nun_x + nun_y * nun_y));
            if (nun_dist > 15.0f) {
              snprintf(buf, sizeof(buf), "Nunchuk stick drift (distance: %.0f)",
                       nun_dist);
              ui_draw_warn(buf);
            }
          }
        }
      }

      ui_printf("\n");

    } else if (ret == WPAD_ERR_NOT_READY) {
      snprintf(buf, sizeof(buf), "Wii Remote %d: Connecting...", chan + 1);
      ui_printf("   " UI_BYELLOW "%s\n" UI_RESET, buf);
    } else {
      snprintf(buf, sizeof(buf), "Wii Remote %d: Not connected", chan + 1);
      ui_printf("   " UI_WHITE "%s\n" UI_RESET, buf);
    }
  }
}

/*---------------------------------------------------------------------------*/
void run_controller_test(void) {
  char buf[64];

  ui_draw_info("Snapshot of controller state.");
  ui_draw_info("Hold buttons during scan to verify they register.");
  ui_printf("\n");

  PAD_ScanPads();
  WPAD_ScanPads();

  test_gc_controllers();
  test_wiimotes();

  /* Summary */
  ui_draw_section("Summary");

  snprintf(buf, sizeof(buf), "%d / 4", s_gc_ports_detected);
  ui_draw_kv("GameCube Ports Active", buf);

  snprintf(buf, sizeof(buf), "%d / 4", s_wiimotes_detected);
  ui_draw_kv("Wii Remotes Connected", buf);

  if (s_gc_ports_detected == 0 && s_wiimotes_detected <= 1)
    ui_draw_info("Connect controllers and re-run to test them");

  ui_printf("\n");
  ui_draw_ok("Controller diagnostics complete");
}

/*---------------------------------------------------------------------------*/
void scan_controllers_quick(void) {
  int warmup, chan, port;

  /* GC controllers */
  s_gc_ports_detected = 0;
  PAD_ScanPads();
  for (port = 0; port < 4; port++) {
    s16 stickX = PAD_StickX(port);
    s16 stickY = PAD_StickY(port);
    u16 btns = PAD_ButtonsHeld(port);
    u8 trigL = PAD_TriggerL(port);
    u8 trigR = PAD_TriggerR(port);
    if (stickX || stickY || btns || trigL || trigR)
      s_gc_ports_detected++;
  }

  /* Wii Remotes - need warmup for BT stack */
  s_wiimotes_detected = 0;
  for (warmup = 0; warmup < 30; warmup++) {
    WPAD_ScanPads();
    VIDEO_WaitVSync();
  }
  for (chan = 0; chan < 4; chan++) {
    u32 type;
    if (WPAD_Probe(chan, &type) == WPAD_ERR_NONE)
      s_wiimotes_detected++;
  }
}

/*---------------------------------------------------------------------------*/
void get_controller_test_report(char *buf, int bufsize) {
  snprintf(buf, bufsize,
           "=== CONTROLLER DIAGNOSTICS ===\n"
           "GameCube Ports Active: %d / 4\n"
           "Wii Remotes Connected: %d / 4\n"
           "\n",
           s_gc_ports_detected, s_wiimotes_detected);
}
