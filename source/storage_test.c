// storage_test.c
// benchmarks SD card and USB drive read/write speeds and pokes around
// at the root directory a bit to see what's there.
//
// the benchmark writes a 1MB temp file and reads it back, repeated 3 times
// and averaged. it's not super scientific but it gives a reasonable ballpark
// for whether your card is fast enough for USB Loader GX and friends.

#include <dirent.h>
#include <fat.h>
#include <gccore.h>
#include <malloc.h>
#include <ogc/lwp_watchdog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "storage_test.h"
#include "ui_common.h"

#define TEST_SIZE       (1024 * 1024)  // 1MB - big enough to get a real measurement
#define BLOCK_SIZE      (32 * 1024)    // 32KB blocks, matches typical SD cluster size
#define ITERATIONS      3              // average over 3 runs to smooth out caching
#define SPEED_GOOD_KB   2000           // >= 2000 KB/s = thumbs up
#define SPEED_OK_KB     1000           // >= 1000 KB/s = acceptable but not great

static char s_report[4096];


static bool device_is_accessible(const char *path) {
    DIR *d = opendir(path);
    if (d) { closedir(d); return true; }
    return false;
}


// looks at the root directory and counts files/folders.
// also checks for /apps since that's the thing everyone actually cares about.
static void show_device_info(const char *name, const char *path) {
    DIR *d = opendir(path);
    if (!d) {
        char msg[128];
        snprintf(msg, sizeof(msg), "%s: not accessible", name);
        ui_draw_err(msg);
        return;
    }

    struct dirent *e;
    int files = 0, dirs = 0;
    char buf[64];

    while ((e = readdir(d)) != NULL) {
        struct stat st;
        char full[512];
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
        if (stat(full, &st) == 0) {
            if (S_ISDIR(st.st_mode)) dirs++;
            else files++;
        }
    }
    closedir(d);

    {
        char msg[128];
        snprintf(msg, sizeof(msg), "%s detected", name);
        ui_draw_ok(msg);
    }

    snprintf(buf, sizeof(buf), "%d files, %d folders", files, dirs);
    ui_draw_kv("Root Contents", buf);

    // check for the apps folder - homebrew users care about this
    {
        char appspath[256];
        snprintf(appspath, sizeof(appspath), "%s/apps", path);
        DIR *ad = opendir(appspath);
        if (ad) {
            int app_count = 0;
            struct dirent *ae;
            while ((ae = readdir(ad)) != NULL) {
                if (ae->d_name[0] != '.') app_count++;
            }
            closedir(ad);
            snprintf(buf, sizeof(buf), "%d apps", app_count);
            ui_draw_kv("/apps", buf);
        }
    }
}


static void run_benchmark(const char *name, const char *base) {
    char testfile[256];
    int blocks = TEST_SIZE / BLOCK_SIZE;
    int i, iter;
    u64 write_ticks = 0, read_ticks = 0;
    float write_kbs, read_kbs;
    char buf[128];

    snprintf(testfile, sizeof(testfile), "%s/wiimedic_bench.tmp", base);

    u8 *data = (u8 *)memalign(32, BLOCK_SIZE);
    if (!data) {
        ui_draw_err("Can't allocate benchmark buffer - out of memory?");
        return;
    }

    // fill with a known pattern so we can theoretically verify reads too
    // (we don't actually verify here but at least the data isn't garbage)
    for (i = 0; i < BLOCK_SIZE; i++)
        data[i] = (u8)(i & 0xFF);

    // --- write test ---
    ui_printf("   " UI_WHITE "Write test...\n" UI_RESET);

    for (iter = 0; iter < ITERATIONS; iter++) {
        FILE *fp = fopen(testfile, "wb");
        if (!fp) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Can't create temp file on %s - is it write protected?", name);
            ui_draw_err(msg);
            free(data);
            return;
        }

        u64 t0 = gettime();
        for (i = 0; i < blocks; i++) {
            if ((int)fwrite(data, 1, BLOCK_SIZE, fp) != BLOCK_SIZE) {
                ui_draw_err("Write error - card may be full or failing");
                fclose(fp);
                free(data);
                remove(testfile);
                return;
            }
        }
        fflush(fp);
        fclose(fp);
        write_ticks += (gettime() - t0);
    }

    {
        float ms = (float)ticks_to_millisecs(write_ticks / ITERATIONS);
        write_kbs = (ms > 0) ? (float)(TEST_SIZE / 1024) * 1000.0f / ms : 0.0f;
    }

    // --- read test ---
    ui_printf("   " UI_WHITE "Read test...\n" UI_RESET);

    for (iter = 0; iter < ITERATIONS; iter++) {
        FILE *fp = fopen(testfile, "rb");
        if (!fp) { ui_draw_err("Can't open temp file for reading"); break; }

        u64 t0 = gettime();
        for (i = 0; i < blocks; i++) {
            if ((int)fread(data, 1, BLOCK_SIZE, fp) != BLOCK_SIZE) {
                ui_draw_err("Read error - storage may be failing");
                fclose(fp);
                free(data);
                remove(testfile);
                return;
            }
        }
        fclose(fp);
        read_ticks += (gettime() - t0);
    }

    {
        float ms = (float)ticks_to_millisecs(read_ticks / ITERATIONS);
        read_kbs = (ms > 0) ? (float)(TEST_SIZE / 1024) * 1000.0f / ms : 0.0f;
    }

    remove(testfile);
    free(data);

    // --- results ---
    const char *wcolor = (write_kbs > SPEED_GOOD_KB) ? UI_BGREEN :
                         (write_kbs > SPEED_OK_KB)   ? UI_BYELLOW : UI_BRED;
    const char *rcolor = (read_kbs  > SPEED_GOOD_KB) ? UI_BGREEN :
                         (read_kbs  > SPEED_OK_KB)   ? UI_BYELLOW : UI_BRED;

    ui_printf("\n");
    snprintf(buf, sizeof(buf), "%.1f KB/s (%.2f MB/s)", write_kbs, write_kbs / 1024.0f);
    ui_draw_kv_color("Write Speed", wcolor, buf);

    snprintf(buf, sizeof(buf), "%.1f KB/s (%.2f MB/s)", read_kbs, read_kbs / 1024.0f);
    ui_draw_kv_color("Read Speed", rcolor, buf);

    if (write_kbs > SPEED_GOOD_KB && read_kbs > SPEED_GOOD_KB) {
        ui_draw_ok("Speed Rating: Excellent - you're good");
    } else if (write_kbs > SPEED_OK_KB && read_kbs > SPEED_OK_KB) {
        ui_draw_warn("Speed Rating: OK - might see occasional load hitches");
    } else {
        ui_draw_err("Speed Rating: Slow - game loading may be affected");
        ui_draw_info("Consider a faster SD card or USB drive");
    }
}


void run_storage_test(void) {
    int rpos = 0;
    bool sd_ok, usb_ok;

    memset(s_report, 0, sizeof(s_report));
    rpos = snprintf(s_report, sizeof(s_report), "=== STORAGE SPEED TEST ===\n");

    sd_ok  = device_is_accessible("sd:/");
    usb_ok = device_is_accessible("usb:/");

    ui_draw_section("SD Card");

    if (sd_ok) {
        show_device_info("SD Card", "sd:/");
        run_benchmark("SD Card", "sd:");
        rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                         "SD Card: Present, benchmark completed\n");
    } else {
        ui_draw_warn("SD Card not found");
        ui_draw_info("Insert an SD card and re-run");
        rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                         "SD Card: Not present\n");
    }

    ui_draw_section("USB Storage");

    if (usb_ok) {
        show_device_info("USB Drive", "usb:/");
        run_benchmark("USB Drive", "usb:");
        rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                         "USB: Present, benchmark completed\n");
    } else {
        ui_printf("   " UI_WHITE "No USB drive detected (that's fine if you don't have one)\n" UI_RESET);
        ui_draw_info("USB must go in the port closest to the edge of the Wii");
        rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                         "USB: Not present\n");
    }

    ui_draw_section("Tips");
    ui_draw_info("Use the bottom USB port (closest to console edge)");
    ui_draw_info("USB 3.0 drives work but only run at 2.0 speeds");
    ui_draw_info("Class 10 / UHS-I SD cards are noticeably faster");
    ui_draw_info("Format USB as FAT32 with 32KB clusters for best results");
    ui_draw_info("SD cards over 32GB need to be formatted as FAT32, not exFAT");

    rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos, "\n");

    ui_printf("\n");
    ui_draw_ok("Storage test complete");
}


void get_storage_test_report(char *buf, int bufsize) {
    strncpy(buf, s_report, bufsize - 1);
    buf[bufsize - 1] = '\0';
}
