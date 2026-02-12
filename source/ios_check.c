/*
 * WiiMedic - ios_check.c
 * Scans all installed IOS versions, detects stubs, patches, and issues
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <gccore.h>

#include "ios_check.h"
#include "ui_common.h"

#define MAX_REPORT 8192

/*---------------------------------------------------------------------------*/
static bool is_known_stub_revision(u32 slot, u32 revision) {
    if (revision == 0) return true;
    switch (slot) {
        case 222: case 223: case 249: case 250:
            return false;
        default: break;
    }
    return false;
}

/*---------------------------------------------------------------------------*/
static const char* get_ios_description(u32 slot) {
    switch (slot) {
        case 9:  case 12: case 13: case 14:
        case 15: case 17: case 21: case 22:
        case 28: return "System Menu";
        case 30: case 31:  return "Channels / WiiConnect24";
        case 33: case 34: case 35: case 37: case 38:
                            return "Used by many games";
        case 36:            return "Used by many games (important!)";
        case 50:            return "System Menu 4.0+";
        case 51:            return "System Menu 4.1+";
        case 52:            return "System Menu / MIOS";
        case 53: case 55: case 56: case 57:
                            return "System Menu 4.2+";
        case 58:            return "System Menu 4.3";
        case 59: case 60: case 61: case 62:
                            return "Used by newer games";
        case 70:            return "System Menu 4.1K+";
        case 80:            return "System Menu 4.3";
        case 222: case 223: return "cIOS (if present)";
        case 236:           return "BootMii IOS";
        case 249: case 250: return "cIOS d2x/Waninkoko";
        case 251:           return "cIOS (if present)";
        case 254:           return "BootMii IOS";
        default:            return "";
    }
}

/* Report state */
static char s_report[MAX_REPORT];
static int  s_total_ios = 0;
static int  s_stub_count = 0;
static int  s_cios_count = 0;

/*---------------------------------------------------------------------------*/
void run_ios_check(void) {
    u32 title_count = 0;
    s32 ret;
    u32 i;
    int rpos = 0;

    ui_draw_info("Scanning installed IOS versions...");
    printf("\n");

    ret = ES_GetNumTitles(&title_count);
    if (ret < 0 || title_count == 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Failed to enumerate titles (error %d)", ret);
        ui_draw_err(msg);
        return;
    }

    u64 *title_list = (u64*)memalign(32, title_count * sizeof(u64));
    if (!title_list) {
        ui_draw_err("Memory allocation failed");
        return;
    }

    ret = ES_GetTitles(title_list, title_count);
    if (ret < 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Failed to get title list (error %d)", ret);
        ui_draw_err(msg);
        free(title_list);
        return;
    }

    /* Table header */
    printf(UI_BCYAN "   %-8s %-12s %-10s %s\n" UI_RESET,
           "IOS", "Revision", "Status", "Notes");
    printf(UI_WHITE "   -------- ------------ ---------- --------------------------\n" UI_RESET);

    s_total_ios = 0;
    s_stub_count = 0;
    s_cios_count = 0;

    memset(s_report, 0, sizeof(s_report));
    rpos = snprintf(s_report, MAX_REPORT,
        "=== IOS INSTALLATION SCAN ===\n"
        "%-8s %-12s %-10s %s\n"
        "-------- ------------ ---------- ----------------------------\n",
        "IOS", "Revision", "Status", "Notes");

    /* Scan IOS titles */
    for (i = 0; i < title_count; i++) {
        u32 title_upper = (u32)(title_list[i] >> 32);
        u32 title_lower = (u32)(title_list[i] & 0xFFFFFFFF);

        if (title_upper != 1) continue;
        if (title_lower < 3 || title_lower > 255) continue;
        if (title_lower == 0x100 || title_lower == 0x101) continue;

        s_total_ios++;

        /* Get revision from TMD */
        u32 tmd_size = 0;
        u32 revision = 0;
        bool is_stub = false;

        ret = ES_GetStoredTMDSize(title_list[i], &tmd_size);
        if (ret >= 0 && tmd_size > 0) {
            signed_blob *tmd_buf = (signed_blob*)memalign(32, tmd_size);
            if (tmd_buf) {
                ret = ES_GetStoredTMD(title_list[i], tmd_buf, tmd_size);
                if (ret >= 0) {
                    tmd *t = SIGNATURE_PAYLOAD(tmd_buf);
                    revision = t->title_version;
                    if (t->num_contents == 0 ||
                        is_known_stub_revision(title_lower, revision))
                        is_stub = true;
                }
                free(tmd_buf);
            }
        }

        /* Status & color */
        const char *status, *color;
        const char *desc = get_ios_description(title_lower);

        if (is_stub) {
            status = "STUB";
            color = UI_BYELLOW;
            s_stub_count++;
        } else if ((title_lower >= 222 && title_lower <= 223) ||
                   (title_lower >= 249 && title_lower <= 251)) {
            status = "cIOS";
            color = UI_BCYAN;
            s_cios_count++;
        } else if (title_lower == 254 || title_lower == 236) {
            status = "BootMii";
            color = UI_BGREEN;
        } else {
            status = "OK";
            color = UI_BGREEN;
        }

        printf("   %sIOS%-4u  rev %-8u %-10s" UI_WHITE " %s\n" UI_RESET,
               color, title_lower, revision, status, desc);

        rpos += snprintf(s_report + rpos, MAX_REPORT - rpos,
            "IOS%-4u  rev %-8u %-10s %s\n",
            title_lower, revision, status, desc);
    }

    free(title_list);

    /* Summary */
    ui_draw_section("Summary");

    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%d", s_total_ios);
        ui_draw_kv("Total IOS Found", buf);

        snprintf(buf, sizeof(buf), "%d", s_total_ios - s_stub_count);
        ui_draw_kv("Active IOS", buf);

        snprintf(buf, sizeof(buf), "%d", s_stub_count);
        if (s_stub_count > 0)
            ui_draw_kv_color("Stub IOS", UI_BYELLOW, buf);
        else
            ui_draw_kv_color("Stub IOS", UI_BGREEN, buf);

        if (s_cios_count > 0) {
            snprintf(buf, sizeof(buf), "%d (cIOS detected)", s_cios_count);
            ui_draw_kv("Custom IOS", buf);
        }
    }

    rpos += snprintf(s_report + rpos, MAX_REPORT - rpos,
        "\nTotal IOS: %d | Active: %d | Stubs: %d | cIOS: %d\n\n",
        s_total_ios, s_total_ios - s_stub_count, s_stub_count, s_cios_count);

    /* Recommendations */
    printf("\n");
    if (s_cios_count > 0) {
        ui_draw_ok("cIOS detected - USB loaders should work properly");
    } else {
        ui_draw_warn("No cIOS found - USB loaders require cIOS d2x");
        ui_draw_info("Install d2x cIOS via d2x cIOS Installer");
    }

    printf("\n");
    ui_draw_ok("IOS scan complete");
}

/*---------------------------------------------------------------------------*/
void get_ios_check_report(char *buf, int bufsize) {
    strncpy(buf, s_report, bufsize - 1);
    buf[bufsize - 1] = '\0';
}
