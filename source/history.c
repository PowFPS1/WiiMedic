/*
 * WiiMedic - history.c
 * Saves timestamped diagnostic snapshots and compares them over time.
 * Data is stored as a simple binary file on SD or USB.
 *
 * Each snapshot records:
 *   - Timestamp (sequential run counter)
 *   - NAND cluster/inode usage
 *   - NAND health score
 *   - IOS count / stub count / cIOS count
 *   - Storage presence (SD/USB)
 *   - Network status
 *   - Controller counts
 *   - Boot2 version, Hollywood revision
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <gccore.h>
#include <ogc/isfs.h>
#include <fat.h>
#include <dirent.h>
#include <wiiuse/wpad.h>

#include "history.h"
#include "ui_common.h"

#define HISTORY_MAGIC      0x574D4843  /* 'WMHC' */
#define HISTORY_VERSION    1
#define MAX_SNAPSHOTS      50
#define HISTORY_PATH_SD    "sd:/WiiMedic_History.dat"
#define HISTORY_PATH_USB   "usb:/WiiMedic_History.dat"

#pragma pack(push, 1)
typedef struct {
    u32 magic;
    u32 version;
    u32 count;
    u32 reserved;
} history_header_t;

typedef struct {
    u32 run_number;          /* sequential run counter */
    /* NAND data */
    u32 nand_clusters_used;
    u32 nand_inodes_used;
    s32 nand_health_score;
    /* IOS data */
    u32 ios_total;
    u32 ios_stubs;
    u32 ios_cios;
    /* System */
    u32 hollywood_rev;
    u32 boot2_ver;
    /* Storage */
    u8  has_sd;
    u8  has_usb;
    /* Network */
    u8  wifi_ok;
    /* Controllers */
    u8  gc_ports;
    u8  wiimotes;
    u8  padding[3];
} snapshot_t;
#pragma pack(pop)

/*---------------------------------------------------------------------------*/
static const char* find_history_path(void) {
    FILE *f = fopen(HISTORY_PATH_SD, "rb");
    if (f) { fclose(f); return HISTORY_PATH_SD; }
    f = fopen(HISTORY_PATH_USB, "rb");
    if (f) { fclose(f); return HISTORY_PATH_USB; }
    /* No existing file - try to create on SD first */
    f = fopen(HISTORY_PATH_SD, "wb");
    if (f) { fclose(f); return HISTORY_PATH_SD; }
    f = fopen(HISTORY_PATH_USB, "wb");
    if (f) { fclose(f); return HISTORY_PATH_USB; }
    return NULL;
}

/*---------------------------------------------------------------------------*/
static int load_history(const char *path, history_header_t *hdr,
                        snapshot_t *snaps, int max_snaps) {
    FILE *f = fopen(path, "rb");
    int count;
    if (!f) return 0;

    if (fread(hdr, sizeof(*hdr), 1, f) != 1 ||
        hdr->magic != HISTORY_MAGIC ||
        hdr->version != HISTORY_VERSION) {
        fclose(f);
        return 0;
    }

    count = (int)hdr->count;
    if (count > max_snaps) count = max_snaps;
    if (count > 0)
        fread(snaps, sizeof(snapshot_t), count, f);

    fclose(f);
    return count;
}

/*---------------------------------------------------------------------------*/
static void save_history(const char *path, history_header_t *hdr,
                         snapshot_t *snaps, int count) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    hdr->magic   = HISTORY_MAGIC;
    hdr->version = HISTORY_VERSION;
    hdr->count   = (u32)count;
    fwrite(hdr, sizeof(*hdr), 1, f);
    fwrite(snaps, sizeof(snapshot_t), count, f);
    fclose(f);
}

/*---------------------------------------------------------------------------*/
static void collect_snapshot(snapshot_t *snap, u32 run_number) {
    u32 title_count = 0;
    u64 *tlist;
    u32 i;
    s32 ret;
    DIR *d;

    memset(snap, 0, sizeof(*snap));
    snap->run_number = run_number;

    /* System info */
    snap->hollywood_rev = SYS_GetHollywoodRevision();
    ES_GetBoot2Version(&snap->boot2_ver);

    /* NAND */
    ret = ISFS_Initialize();
    if (ret >= 0) {
        u32 used_bytes = 0, used_inodes = 0;
        ISFS_GetUsage("/", &used_bytes, &used_inodes);
        snap->nand_clusters_used = used_bytes;
        snap->nand_inodes_used   = used_inodes;

        /* Compute health score inline */
        {
            float cpct = (float)used_bytes * 100.0f / 2048.0f;
            float ipct = (float)used_inodes * 100.0f / 6143.0f;
            int score = 100;
            if (cpct > 95.0f) score -= 30;
            else if (cpct > 85.0f) score -= 15;
            else if (cpct > 75.0f) score -= 5;
            if (ipct > 95.0f) score -= 30;
            else if (ipct > 85.0f) score -= 15;
            else if (ipct > 75.0f) score -= 5;
            if (score < 0) score = 0;
            snap->nand_health_score = score;
        }
        ISFS_Deinitialize();
    } else {
        snap->nand_health_score = -1; /* Could not read */
    }

    /* IOS */
    snap->ios_total = 0;
    snap->ios_stubs = 0;
    snap->ios_cios  = 0;
    if (ES_GetNumTitles(&title_count) >= 0 && title_count > 0) {
        tlist = (u64 *)memalign(32, title_count * sizeof(u64));
        if (tlist && ES_GetTitles(tlist, title_count) >= 0) {
            for (i = 0; i < title_count; i++) {
                u32 upper = (u32)(tlist[i] >> 32);
                u32 lower = (u32)(tlist[i] & 0xFFFFFFFF);
                u32 tmd_size = 0;
                if (upper != 1 || lower < 3 || lower > 255) continue;
                snap->ios_total++;

                if ((lower >= 222 && lower <= 223) ||
                    (lower >= 249 && lower <= 251))
                    snap->ios_cios++;

                if (ES_GetStoredTMDSize(tlist[i], &tmd_size) >= 0 && tmd_size > 0) {
                    signed_blob *tb = (signed_blob *)memalign(32, tmd_size);
                    if (tb) {
                        if (ES_GetStoredTMD(tlist[i], tb, tmd_size) >= 0) {
                            tmd *t = SIGNATURE_PAYLOAD(tb);
                            if (t->title_version == 0 || t->num_contents == 0)
                                snap->ios_stubs++;
                        }
                        free(tb);
                    }
                }
            }
            free(tlist);
        }
    }

    /* Storage */
    d = opendir("sd:/");
    snap->has_sd = (d != NULL) ? 1 : 0;
    if (d) closedir(d);

    d = opendir("usb:/");
    snap->has_usb = (d != NULL) ? 1 : 0;
    if (d) closedir(d);

    /* Network - just check if net_init was successful previously
       We don't want to block for 15 seconds here, so just record 0 */
    snap->wifi_ok = 0;

    /* Controllers */
    {
        int port, chan, warmup;
        snap->gc_ports = 0;
        PAD_ScanPads();
        for (port = 0; port < 4; port++) {
            if (PAD_StickX(port) || PAD_StickY(port) ||
                PAD_ButtonsHeld(port) || PAD_TriggerL(port) || PAD_TriggerR(port))
                snap->gc_ports++;
        }

        snap->wiimotes = 0;
        for (warmup = 0; warmup < 30; warmup++) {
            WPAD_ScanPads();
            VIDEO_WaitVSync();
        }
        for (chan = 0; chan < 4; chan++) {
            u32 type;
            if (WPAD_Probe(chan, &type) == WPAD_ERR_NONE)
                snap->wiimotes++;
        }
    }
}

/*---------------------------------------------------------------------------*/
static void show_trend(const char *label, int old_val, int new_val,
                       bool higher_is_worse) {
    char buf[128];
    const char *indicator;
    const char *color;

    if (old_val == new_val) {
        indicator = "(unchanged)";
        color = UI_WHITE;
    } else if ((new_val > old_val) == higher_is_worse) {
        indicator = "(WORSE)";
        color = UI_BRED;
    } else {
        indicator = "(improved)";
        color = UI_BGREEN;
    }

    snprintf(buf, sizeof(buf), "%d -> %d %s", old_val, new_val, indicator);
    ui_draw_kv_color(label, color, buf);
}

/*---------------------------------------------------------------------------*/
void history_save_snapshot(void) {
    const char *path;
    history_header_t hdr;
    snapshot_t snaps[MAX_SNAPSHOTS];
    int count;
    u32 next_run;
    snapshot_t new_snap;

    path = find_history_path();
    if (!path) {
        ui_draw_warn("No storage available for history tracking");
        return;
    }

    count = load_history(path, &hdr, snaps, MAX_SNAPSHOTS);

    /* Determine next run number */
    if (count > 0)
        next_run = snaps[count - 1].run_number + 1;
    else
        next_run = 1;

    /* Collect current data */
    ui_printf(UI_WHITE "   Saving diagnostic snapshot #%u...\n" UI_RESET, next_run);
    collect_snapshot(&new_snap, next_run);

    /* If full, shift out oldest */
    if (count >= MAX_SNAPSHOTS) {
        memmove(&snaps[0], &snaps[1], sizeof(snapshot_t) * (MAX_SNAPSHOTS - 1));
        count = MAX_SNAPSHOTS - 1;
    }

    snaps[count] = new_snap;
    count++;

    save_history(path, &hdr, snaps, count);
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "Snapshot #%u saved (%d total on record)", next_run, count);
        ui_draw_ok(buf);
    }
}

/*---------------------------------------------------------------------------*/
void run_history(void) {
    const char *path;
    history_header_t hdr;
    snapshot_t snaps[MAX_SNAPSHOTS];
    int count, i;

    path = find_history_path();
    if (!path) {
        ui_draw_err("No history file found on SD or USB");
        ui_printf("\n");
        ui_draw_info("Run 'Generate Full Report' or 'System Checkup'");
        ui_draw_info("at least twice to start tracking changes.");
        return;
    }

    count = load_history(path, &hdr, snaps, MAX_SNAPSHOTS);
    if (count == 0) {
        ui_draw_warn("History file is empty or corrupted");
        ui_draw_info("Run modules and save reports to start building history.");
        return;
    }

    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%d snapshot(s) on record", count);
        ui_draw_info(buf);
        snprintf(buf, sizeof(buf), "File: %s", path);
        ui_draw_info(buf);
    }

    /* Show latest snapshot summary */
    ui_draw_section("Latest Snapshot (#" );
    {
        char buf[64];
        snapshot_t *latest = &snaps[count - 1];
        snprintf(buf, sizeof(buf), "Run #%u", latest->run_number);
        ui_draw_kv("Snapshot", buf);

        snprintf(buf, sizeof(buf), "%u / 2048 (%.1f%%)",
                 latest->nand_clusters_used,
                 (float)latest->nand_clusters_used * 100.0f / 2048.0f);
        ui_draw_kv("NAND Clusters", buf);

        snprintf(buf, sizeof(buf), "%u / 6143", latest->nand_inodes_used);
        ui_draw_kv("NAND Inodes", buf);

        if (latest->nand_health_score >= 0) {
            snprintf(buf, sizeof(buf), "%d / 100", latest->nand_health_score);
            if (latest->nand_health_score >= 80)
                ui_draw_kv_color("Health Score", UI_BGREEN, buf);
            else if (latest->nand_health_score >= 50)
                ui_draw_kv_color("Health Score", UI_BYELLOW, buf);
            else
                ui_draw_kv_color("Health Score", UI_BRED, buf);
        }

        snprintf(buf, sizeof(buf), "%u total, %u stubs, %u cIOS",
                 latest->ios_total, latest->ios_stubs, latest->ios_cios);
        ui_draw_kv("IOS", buf);

        snprintf(buf, sizeof(buf), "SD: %s  USB: %s",
                 latest->has_sd ? "Yes" : "No",
                 latest->has_usb ? "Yes" : "No");
        ui_draw_kv("Storage", buf);
    }

    /* Comparison with previous snapshot (if available) */
    if (count >= 2) {
        snapshot_t *prev = &snaps[count - 2];
        snapshot_t *curr = &snaps[count - 1];
        char buf[64];

        ui_draw_section("Changes Since Previous Run");

        snprintf(buf, sizeof(buf), "Run #%u vs Run #%u",
                 prev->run_number, curr->run_number);
        ui_draw_info(buf);
        ui_printf("\n");

        show_trend("NAND Clusters Used",
                   (int)prev->nand_clusters_used,
                   (int)curr->nand_clusters_used, true);

        show_trend("NAND Inodes Used",
                   (int)prev->nand_inodes_used,
                   (int)curr->nand_inodes_used, true);

        if (prev->nand_health_score >= 0 && curr->nand_health_score >= 0) {
            show_trend("Health Score",
                       prev->nand_health_score,
                       curr->nand_health_score, false);
        }

        show_trend("Total IOS",
                   (int)prev->ios_total,
                   (int)curr->ios_total, false);

        show_trend("Stub IOS",
                   (int)prev->ios_stubs,
                   (int)curr->ios_stubs, true);

        /* Flag significant degradation */
        ui_printf("\n");
        if (curr->nand_clusters_used > prev->nand_clusters_used + 100) {
            ui_draw_warn("NAND usage increased significantly since last run!");
            ui_draw_info("Check if new channels or save data are consuming space.");
        }
        if (curr->nand_health_score >= 0 && prev->nand_health_score >= 0 &&
            curr->nand_health_score < prev->nand_health_score - 10) {
            ui_draw_err("Health score dropped significantly!");
            ui_draw_info("Run System Checkup for detailed recommendations.");
        }
        if (curr->ios_stubs > prev->ios_stubs) {
            ui_draw_warn("More stub IOS detected than before.");
            ui_draw_info("A system update or tool may have stubbed IOS slots.");
        }
    } else {
        ui_printf("\n");
        ui_draw_info("Only 1 snapshot recorded. Run diagnostics again later");
        ui_draw_info("to start seeing trends and comparisons.");
    }

    /* History timeline (compact, last 10) */
    if (count > 1) {
        int start = (count > 10) ? count - 10 : 0;
        char buf[64];

        ui_draw_section("Health Score Timeline");

        ui_printf(UI_BCYAN "   %-6s %-12s %-10s %-10s\n" UI_RESET,
               "Run", "Clusters", "Inodes", "Score");
        ui_printf(UI_WHITE "   ------ ------------ ---------- ----------\n" UI_RESET);

        for (i = start; i < count; i++) {
            const char *color;
            if (snaps[i].nand_health_score >= 80) color = UI_BGREEN;
            else if (snaps[i].nand_health_score >= 50) color = UI_BYELLOW;
            else color = UI_BRED;

            snprintf(buf, sizeof(buf), "%d/100", snaps[i].nand_health_score);
            ui_printf("   %-6u %-12u %-10u %s%s\n" UI_RESET,
                   snaps[i].run_number,
                   snaps[i].nand_clusters_used,
                   snaps[i].nand_inodes_used,
                   color, buf);
        }
    }

    ui_printf("\n");
    ui_draw_ok("History review complete");
}
