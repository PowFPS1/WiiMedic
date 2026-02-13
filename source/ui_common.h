/*
 * WiiMedic - ui_common.h
 * Shared UI definitions, colors, and drawing helpers
 * All characters are ASCII-safe for the Wii's console font
 */

#ifndef _UI_COMMON_H_
#define _UI_COMMON_H_

#include <gccore.h>

/* App version */
#define WIIMEDIC_VERSION "1.0.0"

/* ANSI Color Codes - supported by libogc console */
#define UI_RESET    "\x1b[0m"

/* Standard foreground colors */
#define UI_RED      "\x1b[31m"
#define UI_GREEN    "\x1b[32m"
#define UI_YELLOW   "\x1b[33m"
#define UI_BLUE     "\x1b[34m"
#define UI_MAGENTA  "\x1b[35m"
#define UI_CYAN     "\x1b[36m"
#define UI_WHITE    "\x1b[37m"

/* Bright / bold foreground colors */
#define UI_BRED     "\x1b[31;1m"
#define UI_BGREEN   "\x1b[32;1m"
#define UI_BYELLOW  "\x1b[33;1m"
#define UI_BBLUE    "\x1b[34;1m"
#define UI_BMAGENTA "\x1b[35;1m"
#define UI_BCYAN    "\x1b[36;1m"
#define UI_BWHITE   "\x1b[37;1m"

/* Clear the screen and reset cursor to top-left */
void ui_clear(void);

/* Draw the WiiMedic banner at the top of the screen */
void ui_draw_banner(void);

/* Draw a horizontal divider line */
void ui_draw_line(void);

/* Draw a section header:  --- Title --- */
void ui_draw_section(const char *title);

/* Draw a key-value pair:  Label ............ Value */
void ui_draw_kv(const char *label, const char *value);

/* Draw a key-value pair with a colored value */
void ui_draw_kv_color(const char *label, const char *color, const char *value);

/* Draw a progress bar: [####..........] 45.2%  */
void ui_draw_bar(u32 used, u32 total, int bar_width);

/* Status messages with indicator prefix */
void ui_draw_ok(const char *msg);
void ui_draw_warn(const char *msg);
void ui_draw_err(const char *msg);
void ui_draw_info(const char *msg);

/* Draw footer with navigation hint (NULL = default nav text) */
void ui_draw_footer(const char *msg);

/* Print that routes through scroll buffer when active */
int ui_printf(const char *fmt, ...);

/* Start capturing output to scroll buffer */
void ui_scroll_begin(void);

/* Display scroll viewer with UP/DOWN/LEFT/RIGHT navigation */
void ui_scroll_view(const char *title);

/* Wait for A or B button press */
void ui_wait_button(void);

#endif /* _UI_COMMON_H_ */
