/*
 * WiiMedic - ui_common.c
 * Shared UI drawing functions - ASCII-safe for the Wii console font
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <gccore.h>
#include <wiiuse/wpad.h>

#include "ui_common.h"

#define LINE_WIDTH 60

/* Scroll buffer system */
#define SCROLL_MAX_LINES 256
#define SCROLL_LINE_LEN  512
#define SCROLL_VISIBLE   18

static char s_scroll_lines[SCROLL_MAX_LINES][SCROLL_LINE_LEN];
static int  s_scroll_count = 0;
static char s_scroll_cur[SCROLL_LINE_LEN];
static int  s_scroll_pos = 0;
static bool s_scroll_active = false;

/*---------------------------------------------------------------------------*/
int ui_printf(const char *fmt, ...) {
    va_list args;
    char tmp[512];
    int len, i;

    va_start(args, fmt);
    if (!s_scroll_active) {
        len = vprintf(fmt, args);
        va_end(args);
        return len;
    }

    len = vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);

    for (i = 0; i < len && tmp[i]; i++) {
        if (tmp[i] == '\n') {
            s_scroll_cur[s_scroll_pos] = '\0';
            if (s_scroll_count < SCROLL_MAX_LINES) {
                memcpy(s_scroll_lines[s_scroll_count], s_scroll_cur,
                       s_scroll_pos + 1);
                s_scroll_count++;
            }
            s_scroll_pos = 0;
            s_scroll_cur[0] = '\0';
        } else {
            if (s_scroll_pos < SCROLL_LINE_LEN - 1) {
                s_scroll_cur[s_scroll_pos++] = tmp[i];
            }
        }
    }
    return len;
}

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
    ui_printf("  " UI_WHITE);
    for (i = 0; i < LINE_WIDTH; i++) ui_printf("-");
    ui_printf("\n" UI_RESET);
}

/*---------------------------------------------------------------------------*/
void ui_draw_section(const char *title) {
    ui_printf("\n" UI_BCYAN "   --- %s ---\n\n" UI_RESET, title);
}

/*---------------------------------------------------------------------------*/
void ui_draw_kv(const char *label, const char *value) {
    int label_len = (int)strlen(label);
    int dots = 30 - label_len;
    int i;
    if (dots < 2) dots = 2;

    ui_printf("   " UI_CYAN "%s " UI_RESET, label);
    for (i = 0; i < dots; i++) ui_printf(".");
    ui_printf(" " UI_BWHITE "%s\n" UI_RESET, value);
}

/*---------------------------------------------------------------------------*/
void ui_draw_kv_color(const char *label, const char *color, const char *value) {
    int label_len = (int)strlen(label);
    int dots = 30 - label_len;
    int i;
    if (dots < 2) dots = 2;

    ui_printf("   " UI_CYAN "%s " UI_RESET, label);
    for (i = 0; i < dots; i++) ui_printf(".");
    ui_printf(" %s%s\n" UI_RESET, color, value);
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

    ui_printf("   [");
    for (i = 0; i < bar_width; i++) {
        if (i < filled)
            ui_printf("%s#" UI_RESET, color);
        else
            ui_printf(UI_WHITE "." UI_RESET);
    }
    ui_printf("] %s%.1f%%\n" UI_RESET, color, pct);
}

/*---------------------------------------------------------------------------*/
void ui_draw_ok(const char *msg) {
    ui_printf("   " UI_BGREEN "[OK]" UI_RESET " %s\n", msg);
}

void ui_draw_warn(const char *msg) {
    ui_printf("   " UI_BYELLOW "[!!]" UI_RESET " %s\n", msg);
}

void ui_draw_err(const char *msg) {
    ui_printf("   " UI_BRED "[XX]" UI_RESET " %s\n", msg);
}

void ui_draw_info(const char *msg) {
    ui_printf("   " UI_BCYAN "(i)" UI_RESET "  %s\n", msg);
}

/*---------------------------------------------------------------------------*/
void ui_scroll_begin(void) {
    s_scroll_count = 0;
    s_scroll_pos = 0;
    s_scroll_cur[0] = '\0';
    s_scroll_active = true;
}

/*---------------------------------------------------------------------------*/
void ui_scroll_view(const char *title) {
    int offset = 0;
    int max_offset;
    int visible = SCROLL_VISIBLE;

    /* Flush any remaining partial line */
    if (s_scroll_pos > 0) {
        s_scroll_cur[s_scroll_pos] = '\0';
        if (s_scroll_count < SCROLL_MAX_LINES) {
            memcpy(s_scroll_lines[s_scroll_count], s_scroll_cur,
                   s_scroll_pos + 1);
            s_scroll_count++;
        }
        s_scroll_pos = 0;
    }
    s_scroll_active = false;

    max_offset = s_scroll_count - visible;
    if (max_offset < 0) max_offset = 0;

    /* Initial full clear */
    printf("\x1b[2J\x1b[0;0H");

    while (1) {
        int i, end;
        u32 wpad, gpad;
        bool redraw;

        /* Reposition cursor to top-left (no clear) */
        printf("\x1b[0;0H");

        /* Compact header */
        printf(UI_BGREEN " [+] WiiMedic" UI_RESET " " UI_CYAN "v"
               WIIMEDIC_VERSION UI_RESET "  " UI_BWHITE "%s" UI_RESET
               "\x1b[K\n",
               title);
        printf(UI_WHITE " ");
        for (i = 0; i < 58; i++) printf("-");
        printf("\x1b[K\n" UI_RESET);

        /* Content lines */
        end = offset + visible;
        if (end > s_scroll_count) end = s_scroll_count;
        for (i = offset; i < end; i++) {
            printf(UI_RESET "%s\x1b[K\n", s_scroll_lines[i]);
        }
        /* Pad so footer stays at bottom */
        for (i = end - offset; i < visible; i++) printf("\x1b[K\n");

        /* Footer */
        printf(UI_WHITE " ");
        for (i = 0; i < 58; i++) printf("-");
        printf("\x1b[K\n" UI_RESET);
        if (max_offset > 0) {
            printf(UI_WHITE " [UP/DOWN] Scroll  [LEFT/RIGHT] Page  "
                   "[A/B] Return" UI_RESET
                   UI_CYAN "  [%d-%d/%d]" UI_RESET "\x1b[K\n",
                   offset + 1, end, s_scroll_count);
        } else {
            printf(UI_WHITE
                   " Press [A] or [B] to return to menu...\x1b[K\n" UI_RESET);
        }

        VIDEO_WaitVSync();

        /* Input loop */
        while (1) {
            WPAD_ScanPads();
            PAD_ScanPads();
            wpad = WPAD_ButtonsDown(0);
            gpad = PAD_ButtonsDown(0);
            redraw = false;

            if ((wpad & WPAD_BUTTON_UP) || (gpad & PAD_BUTTON_UP)) {
                if (offset > 0) { offset--; redraw = true; }
            }
            if ((wpad & WPAD_BUTTON_DOWN) || (gpad & PAD_BUTTON_DOWN)) {
                if (offset < max_offset) { offset++; redraw = true; }
            }
            if ((wpad & WPAD_BUTTON_LEFT) || (gpad & PAD_BUTTON_LEFT)) {
                offset -= visible;
                if (offset < 0) offset = 0;
                redraw = true;
            }
            if ((wpad & WPAD_BUTTON_RIGHT) || (gpad & PAD_BUTTON_RIGHT)) {
                offset += visible;
                if (offset > max_offset) offset = max_offset;
                redraw = true;
            }
            if ((wpad & WPAD_BUTTON_A) || (wpad & WPAD_BUTTON_B) ||
                (gpad & PAD_BUTTON_A) || (gpad & PAD_BUTTON_B)) {
                return;
            }

            if (redraw) break;
            VIDEO_WaitVSync();
        }
    }
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
