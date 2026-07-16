// nand_health.c
// scans the Wii NAND, reports usage, and gives it a health score out of 100.
// the health score is a bit made up but it gives people a quick summary
// without having to read through all the raw numbers.

#include <gccore.h>
#include <ogc/isfs.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nand_health.h"
#include "ui_common.h"

// Wii NAND layout: 512MB flash, 32768 clusters at 16KB each, 6143 inodes max.
// these are fixed hardware limits, not filesystem limits.
#define NAND_TOTAL_CLUSTERS 32768
#define NAND_TOTAL_INODES   6143

static u32  s_used_blocks   = 0;
static u32  s_free_blocks   = 0;
static u32  s_used_inodes   = 0;
static u32  s_free_inodes   = 0;
static int  s_health_score  = 100;
static char s_health_status[64] = "Unknown";
static int  s_title_count   = 0;
static int  s_ticket_count  = 0;
static bool s_nand_run      = false;


bool has_nand_health_run(void) { return s_nand_run; }


// counts entries in a NAND directory. returns -1 if access is denied
// (which happens on /sys pretty much always unless you're running a very
// permissive cIOS)
static int count_nand_entries(const char *path) {
    char pbuf[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
    u32 count = 0;

    strncpy(pbuf, path, ISFS_MAXPATH - 1);
    pbuf[ISFS_MAXPATH - 1] = '\0';

    s32 ret = ISFS_ReadDir(pbuf, NULL, &count);
    if (ret < 0) return -1;
    return (int)count;
}


void run_nand_health(void) {
    s32 ret;
    float cluster_pct = 0.0f, inode_pct = 0.0f;

    ui_draw_info("Initializing NAND filesystem scan...");
    ui_printf("\n");

    ret = ISFS_Initialize();
    bool we_opened = (ret >= 0);
    if (ret < 0 && ret != -105) { // -105 = already initialized
        ui_draw_err("Failed to initialize ISFS");
        char msg[64];
        snprintf(msg, sizeof(msg), "Error code: %d", ret);
        ui_draw_info(msg);
        ui_draw_warn("Try running on IOS58 or a cIOS that allows NAND access");
        return;
    }

    // ISFS_GetUsage returns FREE counts, not used.
    // subtract from totals to get the actual used numbers.
    u32 free_clusters = 0, free_inodes = 0;
    ret = ISFS_GetUsage("/", &free_clusters, &free_inodes);
    if (ret >= 0) {
        s_free_blocks = (free_clusters <= NAND_TOTAL_CLUSTERS) ? free_clusters : 0;
        s_free_inodes = (free_inodes   <= NAND_TOTAL_INODES)   ? free_inodes   : 0;
        s_used_blocks = NAND_TOTAL_CLUSTERS - s_free_blocks;
        s_used_inodes = NAND_TOTAL_INODES   - s_free_inodes;
    } else {
        // GetUsage failing isn't fatal, we can still do the dir scan
        char errmsg[80];
        snprintf(errmsg, sizeof(errmsg), "ISFS_GetUsage failed (%d) - space data unavailable", ret);
        ui_draw_warn(errmsg);
        s_free_blocks = s_free_inodes = 0;
        s_used_blocks = s_used_inodes = 0;
    }

    ui_draw_section("NAND Storage Usage");
    {
        char buf[64];

        snprintf(buf, sizeof(buf), "%u / %u clusters", s_used_blocks, (u32)NAND_TOTAL_CLUSTERS);
        ui_draw_kv("Clusters Used", buf);

        // 16 KB per cluster, convert to MB for the free display
        snprintf(buf, sizeof(buf), "%u clusters (%.1f MB free)",
                 s_free_blocks, (float)s_free_blocks * 16.0f / 1024.0f);
        ui_draw_kv("Clusters Free", buf);
    }

    ui_printf("\n   Cluster Usage:\n");
    ui_draw_bar(s_used_blocks, NAND_TOTAL_CLUSTERS, 40);

    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%u / %u", s_used_inodes, (u32)NAND_TOTAL_INODES);
        ui_draw_kv("Inodes Used", buf);

        snprintf(buf, sizeof(buf), "%u", s_free_inodes);
        ui_draw_kv("Inodes Free", buf);
    }

    ui_printf("\n   Inode Usage:\n");
    ui_draw_bar(s_used_inodes, NAND_TOTAL_INODES, 40);

    ui_draw_section("NAND Directory Scan");
    {
        int sys_cnt    = count_nand_entries("/sys");
        int ticket_cnt = count_nand_entries("/ticket");
        int title_cnt  = count_nand_entries("/title");
        int shared_cnt = count_nand_entries("/shared1");
        int tmp_cnt    = count_nand_entries("/tmp");
        int import_cnt = count_nand_entries("/import");
        char buf[64];

        s_title_count  = title_cnt;
        s_ticket_count = ticket_cnt;

        if (sys_cnt >= 0) {
            snprintf(buf, sizeof(buf), "%d entries", sys_cnt);
            ui_draw_kv("/sys", buf);
        } else {
            ui_draw_kv_color("/sys", UI_BRED, "Access denied (normal)");
        }

        if (ticket_cnt >= 0) {
            snprintf(buf, sizeof(buf), "%d title ticket groups", ticket_cnt);
            ui_draw_kv("/ticket", buf);
        }

        if (title_cnt >= 0) {
            snprintf(buf, sizeof(buf), "%d title categories", title_cnt);
            ui_draw_kv("/title", buf);
        }

        if (shared_cnt >= 0) {
            snprintf(buf, sizeof(buf), "%d shared contents", shared_cnt);
            ui_draw_kv("/shared1", buf);
        }

        if (tmp_cnt >= 0) {
            snprintf(buf, sizeof(buf), "%d entries", tmp_cnt);
            ui_draw_kv("/tmp", buf);
            if (tmp_cnt > 10)
                ui_draw_warn("/tmp has a lot of files - interrupted install maybe?");
        }

        // /import being non-empty means something got interrupted mid-install.
        // not dangerous on its own but worth flagging.
        if (import_cnt > 0) {
            snprintf(buf, sizeof(buf), "%d entries", import_cnt);
            ui_draw_kv_color("/import", UI_BYELLOW, buf);
            ui_draw_warn("Import dir not empty - looks like an install got interrupted");
        } else if (import_cnt == 0) {
            ui_draw_kv_color("/import", UI_BGREEN, "Empty (good)");
        }

        // health score calculation. completely arbitrary thresholds but
        // they seem reasonable. dock points for high usage and leftover junk.
        s_health_score = 100;
        cluster_pct = (float)s_used_blocks * 100.0f / (float)NAND_TOTAL_CLUSTERS;
        inode_pct   = (float)s_used_inodes * 100.0f / (float)NAND_TOTAL_INODES;

        if      (cluster_pct > 95.0f) s_health_score -= 30;
        else if (cluster_pct > 85.0f) s_health_score -= 15;
        else if (cluster_pct > 75.0f) s_health_score -= 5;

        if      (inode_pct > 95.0f)   s_health_score -= 30;
        else if (inode_pct > 85.0f)   s_health_score -= 15;
        else if (inode_pct > 75.0f)   s_health_score -= 5;

        if (import_cnt > 0) s_health_score -= 10;
        if (tmp_cnt > 10)   s_health_score -= 5;
        if (s_health_score < 0) s_health_score = 0;
    }

    {
        char msg[128];
        if (s_health_score >= 80) {
            strcpy(s_health_status, "GOOD");
            snprintf(msg, sizeof(msg), "NAND Health Score: %d/100 - %s", s_health_score, s_health_status);
            ui_printf("\n");
            ui_draw_ok(msg);
        } else if (s_health_score >= 50) {
            strcpy(s_health_status, "FAIR - keep an eye on it");
            snprintf(msg, sizeof(msg), "NAND Health Score: %d/100 - %s", s_health_score, s_health_status);
            ui_printf("\n");
            ui_draw_warn(msg);
        } else {
            strcpy(s_health_status, "POOR - action recommended");
            snprintf(msg, sizeof(msg), "NAND Health Score: %d/100 - %s", s_health_score, s_health_status);
            ui_printf("\n");
            ui_draw_err(msg);
        }
    }

    if (cluster_pct > 85.0f)
        ui_draw_info("Lots of NAND used - consider deleting unused channels");
    if (inode_pct > 85.0f)
        ui_draw_info("Running low on inodes - lots of small files on NAND");

    ui_printf("\n");
    ui_draw_ok("NAND health check complete");

    if (we_opened) ISFS_Deinitialize();
    s_nand_run = true;
}


void get_nand_health_report(char *buf, int bufsize) {
    snprintf(buf, bufsize,
             "=== NAND HEALTH CHECK ===\n"
             "Clusters Used:       %u / %u\n"
             "Clusters Free:       %u\n"
             "Inodes Used:         %u / %u\n"
             "Inodes Free:         %u\n"
             "Title Categories:    %d\n"
             "Ticket Groups:       %d\n"
             "Health Score:        %d/100\n"
             "Status:              %s\n"
             "\n",
             s_used_blocks, (u32)NAND_TOTAL_CLUSTERS, s_free_blocks,
             s_used_inodes, (u32)NAND_TOTAL_INODES,   s_free_inodes,
             s_title_count, s_ticket_count,
             s_health_score, s_health_status);
}
