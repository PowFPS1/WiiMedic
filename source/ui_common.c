/*
 * WiiMedic - ui_common.c
 * Shared UI drawing functions - ASCII-safe for the Wii console font
 */

#include <stdio.h>
#include <string.h>
#include <gccore.h>
#include <wiiuse/wpad.h>

#include "ui_common.h"

#define LINE_WIDTH 60

/*---------------------------------------------------------------------------*/
void ui_clear(void) {
    printf("\x1b[2J\x1b[0;0H");
}

/*---------------------------------------------------------------------------*/
void ui_draw_banner(void) {
    printf("\n");
    printf(UI_BGREEN
        "  ==========================================================\n"
        UI_RESET);
    printf("\n");
    printf(UI_BWHITE
        "          [+]  W i i M e d i c"
        UI_RESET "   " UI_CYAN "v" WIIMEDIC_VERSION "\n"
        UI_RESET);
    printf("\n");
    printf(UI_WHITE
        "          System Diagnostic & Health Monitor\n"
        UI_RESET);
    printf("\n");
    printf(UI_BGREEN
        "  ==========================================================\n"
        UI_RESET);
    printf("\n");
}

/*---------------------------------------------------------------------------*/
void ui_draw_line(void) {
    int i;
    printf("  " UI_WHITE);
    for (i = 0; i < LINE_WIDTH; i++) printf("-");
    printf("\n" UI_RESET);
}

/*---------------------------------------------------------------------------*/
void ui_draw_section(const char *title) {
    printf("\n" UI_BCYAN "   --- %s ---\n\n" UI_RESET, title);
}

/*---------------------------------------------------------------------------*/
void ui_draw_kv(const char *label, const char *value) {
    int label_len = (int)strlen(label);
    int dots = 30 - label_len;
    int i;
    if (dots < 2) dots = 2;

    printf("   " UI_CYAN "%s " UI_RESET, label);
    for (i = 0; i < dots; i++) printf(".");
    printf(" " UI_BWHITE "%s\n" UI_RESET, value);
}

/*---------------------------------------------------------------------------*/
void ui_draw_kv_color(const char *label, const char *color, const char *value) {
    int label_len = (int)strlen(label);
    int dots = 30 - label_len;
    int i;
    if (dots < 2) dots = 2;

    printf("   " UI_CYAN "%s " UI_RESET, label);
    for (i = 0; i < dots; i++) printf(".");
    printf(" %s%s\n" UI_RESET, color, value);
}

/*---------------------------------------------------------------------------*/
void ui_draw_bar(u32 used, u32 total, int bar_width) {
    int filled = 0;
    float pct = 0.0f;
    const char *color;
    int i;

    if (total > 0) {
        filled = (int)((u64)used * bar_width / total);
        pct = (float)used * 100.0f / (float)total;
    }
    if (filled > bar_width) filled = bar_width;

    if (pct > 90.0f)      color = UI_BRED;
    else if (pct > 70.0f) color = UI_BYELLOW;
    else                   color = UI_BGREEN;

    printf("   [");
    for (i = 0; i < bar_width; i++) {
        if (i < filled)
            printf("%s#" UI_RESET, color);
        else
            printf(UI_WHITE "." UI_RESET);
    }
    printf("] %s%.1f%%\n" UI_RESET, color, pct);
}

/*---------------------------------------------------------------------------*/
void ui_draw_ok(const char *msg) {
    printf("   " UI_BGREEN "[OK]" UI_RESET " %s\n", msg);
}

void ui_draw_warn(const char *msg) {
    printf("   " UI_BYELLOW "[!!]" UI_RESET " %s\n", msg);
}

void ui_draw_err(const char *msg) {
    printf("   " UI_BRED "[XX]" UI_RESET " %s\n", msg);
}

void ui_draw_info(const char *msg) {
    printf("   " UI_BCYAN "(i)" UI_RESET "  %s\n", msg);
}

/*---------------------------------------------------------------------------*/
void ui_draw_footer(const char *msg) {
    printf("\n");
    ui_draw_line();
    if (msg)
        printf("   " UI_WHITE "%s\n" UI_RESET, msg);
    else
        printf("   " UI_WHITE "[UP/DOWN] Navigate   [A] Select   [HOME] Exit\n" UI_RESET);
}

/*---------------------------------------------------------------------------*/
void ui_wait_button(void) {
    printf("\n   " UI_WHITE "Press [A] or [B] to return to menu..." UI_RESET "\n");

    while (1) {
        WPAD_ScanPads();
        PAD_ScanPads();

        u32 wpad = WPAD_ButtonsDown(0);
        u32 gpad = PAD_ButtonsDown(0);

        if ((wpad & WPAD_BUTTON_A) || (wpad & WPAD_BUTTON_B) ||
            (gpad & PAD_BUTTON_A) || (gpad & PAD_BUTTON_B)) {
            break;
        }

        VIDEO_WaitVSync();
    }
}
