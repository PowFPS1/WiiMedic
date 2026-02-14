/*
 * WiiMedic - nand_health.c
 * NAND filesystem health check - scans for usage, free space, and issues
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <ogc/isfs.h>

#include "nand_health.h"
#include "ui_common.h"

/* NAND constants (Wii: 4096 blocks * 8 clusters/block = 32768 clusters) */
#define NAND_TOTAL_CLUSTERS 32768
#define NAND_TOTAL_INODES   6143

/* State for report */
static u32  s_used_inodes  = 0;
static u32  s_free_inodes  = 0;
static u32  s_used_blocks  = 0;
static u32  s_free_blocks  = 0;
static int  s_health_score = 100;
static char s_health_status[64] = "Unknown";
static int  s_title_count  = 0;
static int  s_ticket_count = 0;

/*---------------------------------------------------------------------------*/
static int count_nand_entries(const char *path) {
    char pathbuf[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
    u32 count = 0;

    strncpy(pathbuf, path, ISFS_MAXPATH - 1);
    pathbuf[ISFS_MAXPATH - 1] = '\0';

    s32 ret = ISFS_ReadDir(pathbuf, NULL, &count);
    if (ret < 0) return -1;
    return (int)count;
}

/*---------------------------------------------------------------------------*/
void run_nand_health(void) {
    s32 ret;
    float cluster_pct, inode_pct;

    ui_draw_info("Initializing NAND filesystem scan...");
    ui_printf("\n");

    ret = ISFS_Initialize();
    if (ret < 0) {
        ui_draw_err("Failed to initialize ISFS");
        char msg[64];
        snprintf(msg, sizeof(msg), "Error code: %d", ret);
        ui_draw_info(msg);
        ui_draw_warn("NAND access may require a different IOS");
        return;
    }

    /* Get NAND filesystem usage (returns used clusters, used inodes) */
    u32 used_clusters = 0, used_inodes = 0;

    ret = ISFS_GetUsage("/", &used_clusters, &used_inodes);
    if (ret >= 0) {
        s_used_blocks  = used_clusters;
        s_used_inodes  = used_inodes;
    }

    s_free_inodes = NAND_TOTAL_INODES - s_used_inodes;
    s_free_blocks = (s_used_blocks <= NAND_TOTAL_CLUSTERS)
                        ? (NAND_TOTAL_CLUSTERS - s_used_blocks)
                        : 0;

    /* Storage usage */
    ui_draw_section("NAND Storage Usage");

    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%u / %u clusters", s_used_blocks, (u32)NAND_TOTAL_CLUSTERS);
        ui_draw_kv("Clusters Used", buf);

        snprintf(buf, sizeof(buf), "%u clusters (%.1f MB)", s_free_blocks,
                 (float)s_free_blocks * 16.0f / 1024.0f);  /* 16 KB per cluster */
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

    /* Directory scan */
    ui_draw_section("NAND Directory Scan");

    {
        int sys_count    = count_nand_entries("/sys");
        int ticket_count = count_nand_entries("/ticket");
        int title_count  = count_nand_entries("/title");
        int shared_count = count_nand_entries("/shared1");
        int tmp_count    = count_nand_entries("/tmp");
        int import_count = count_nand_entries("/import");
        char buf[64];

        s_title_count  = title_count;
        s_ticket_count = ticket_count;

        if (sys_count >= 0) {
            snprintf(buf, sizeof(buf), "%d entries", sys_count);
            ui_draw_kv("/sys", buf);
        } else {
            ui_draw_kv_color("/sys", UI_BRED, "Access denied");
        }

        if (ticket_count >= 0) {
            snprintf(buf, sizeof(buf), "%d title ticket groups", ticket_count);
            ui_draw_kv("/ticket", buf);
        }

        if (title_count >= 0) {
            snprintf(buf, sizeof(buf), "%d title categories", title_count);
            ui_draw_kv("/title", buf);
        }

        if (shared_count >= 0) {
            snprintf(buf, sizeof(buf), "%d shared contents", shared_count);
            ui_draw_kv("/shared1", buf);
        }

        if (tmp_count >= 0) {
            snprintf(buf, sizeof(buf), "%d entries", tmp_count);
            ui_draw_kv("/tmp", buf);
            if (tmp_count > 10)
                ui_draw_warn("Temp has many files - may indicate interrupted ops");
        }

        if (import_count >= 0 && import_count > 0) {
            snprintf(buf, sizeof(buf), "%d entries", import_count);
            ui_draw_kv_color("/import", UI_BYELLOW, buf);
            ui_draw_warn("Import not empty - interrupted install detected!");
        } else if (import_count >= 0) {
            ui_draw_kv_color("/import", UI_BGREEN, "Empty (OK)");
        }

        /* Calculate health score */
        s_health_score = 100;
        cluster_pct = (float)s_used_blocks * 100.0f / (float)NAND_TOTAL_CLUSTERS;
        inode_pct   = (float)s_used_inodes * 100.0f / (float)NAND_TOTAL_INODES;

        if (cluster_pct > 95.0f)      s_health_score -= 30;
        else if (cluster_pct > 85.0f) s_health_score -= 15;
        else if (cluster_pct > 75.0f) s_health_score -= 5;

        if (inode_pct > 95.0f)      s_health_score -= 30;
        else if (inode_pct > 85.0f) s_health_score -= 15;
        else if (inode_pct > 75.0f) s_health_score -= 5;

        if (import_count > 0) s_health_score -= 10;
        if (tmp_count > 10)   s_health_score -= 5;
        if (s_health_score < 0) s_health_score = 0;
    }

    /* Health score display */
    {
        char score_msg[128];

        if (s_health_score >= 80) {
            strcpy(s_health_status, "GOOD");
            snprintf(score_msg, sizeof(score_msg),
                     "NAND Health Score: %d/100 - %s", s_health_score, s_health_status);
            ui_printf("\n");
            ui_draw_ok(score_msg);
        } else if (s_health_score >= 50) {
            strcpy(s_health_status, "FAIR - Monitor closely");
            snprintf(score_msg, sizeof(score_msg),
                     "NAND Health Score: %d/100 - %s", s_health_score, s_health_status);
            ui_printf("\n");
            ui_draw_warn(score_msg);
        } else {
            strcpy(s_health_status, "POOR - Action recommended");
            snprintf(score_msg, sizeof(score_msg),
                     "NAND Health Score: %d/100 - %s", s_health_score, s_health_status);
            ui_printf("\n");
            ui_draw_err(score_msg);
        }
    }

    /* Recommendations */
    {
        float cluster_pct2 = (float)s_used_blocks * 100.0f / (float)NAND_TOTAL_CLUSTERS;
        float inode_pct2   = (float)s_used_inodes * 100.0f / (float)NAND_TOTAL_INODES;

        if (cluster_pct2 > 85.0f)
            ui_draw_info("Consider removing unused channels to free space");
        if (inode_pct2 > 85.0f)
            ui_draw_info("Too many files on NAND - consider cleanup");
    }

    ui_printf("\n");
    ui_draw_ok("NAND health check complete");

    ISFS_Deinitialize();
}

/*---------------------------------------------------------------------------*/
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
        s_used_blocks, (u32)NAND_TOTAL_CLUSTERS,
        s_free_blocks,
        s_used_inodes, (u32)NAND_TOTAL_INODES,
        s_free_inodes,
        s_title_count, s_ticket_count,
        s_health_score, s_health_status
    );
}
