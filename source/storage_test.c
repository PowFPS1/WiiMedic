/*
 * WiiMedic - storage_test.c
 * Benchmarks SD card and USB drive read/write speeds
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <gccore.h>
#include <fat.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ogc/lwp_watchdog.h>

#include "storage_test.h"
#include "ui_common.h"

/* Test parameters */
#define TEST_FILE_SIZE    (1024 * 1024)  /* 1 MB */
#define TEST_BLOCK_SIZE   (32 * 1024)    /* 32 KB */
#define TEST_ITERATIONS   3
#define SPEED_GOOD_KB     2000
#define SPEED_OK_KB       1000

static char s_report[4096];

/*---------------------------------------------------------------------------*/
static bool check_device_present(const char *path) {
    DIR *dir = opendir(path);
    if (dir) { closedir(dir); return true; }
    return false;
}

/*---------------------------------------------------------------------------*/
static void get_device_info(const char *device_name, const char *path) {
    DIR *dir = opendir(path);
    struct dirent *entry;
    int file_count = 0, dir_count = 0;
    char buf[64];

    if (!dir) {
        char msg[128];
        snprintf(msg, sizeof(msg), "%s not detected or not accessible", device_name);
        ui_draw_err(msg);
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        struct stat st;
        char fullpath[256];
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
        if (stat(fullpath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) dir_count++;
            else file_count++;
        }
    }
    closedir(dir);

    {
        char msg[128];
        snprintf(msg, sizeof(msg), "%s detected", device_name);
        ui_draw_ok(msg);
    }

    snprintf(buf, sizeof(buf), "%d files, %d folders", file_count, dir_count);
    ui_draw_kv("Root Contents", buf);

    /* Check for /apps */
    {
        char appspath[256];
        snprintf(appspath, sizeof(appspath), "%s/apps", path);
        DIR *apps = opendir(appspath);
        if (apps) {
            int app_count = 0;
            while ((entry = readdir(apps)) != NULL) {
                if (entry->d_name[0] != '.') app_count++;
            }
            closedir(apps);
            snprintf(buf, sizeof(buf), "%d homebrew apps found", app_count);
            ui_draw_kv("Apps Directory", buf);
        }
    }
}

/*---------------------------------------------------------------------------*/
static void run_benchmark(const char *device_name, const char *base_path) {
    char testpath[256];
    int blocks = TEST_FILE_SIZE / TEST_BLOCK_SIZE;
    int i, iter;
    u64 write_total_ticks = 0, read_total_ticks = 0;
    float write_speed_kbs, read_speed_kbs;
    const char *write_color, *read_color, *rating;
    char buf[128];

    snprintf(testpath, sizeof(testpath), "%s/wiimedic_benchmark.tmp", base_path);

    u8 *buffer = (u8*)memalign(32, TEST_BLOCK_SIZE);
    if (!buffer) {
        ui_draw_err("Memory allocation failed for benchmark");
        return;
    }

    for (i = 0; i < TEST_BLOCK_SIZE; i++)
        buffer[i] = (u8)(i & 0xFF);

    /* Write speed */
    ui_printf("   " UI_WHITE "Running write speed test...\n" UI_RESET);

    for (iter = 0; iter < TEST_ITERATIONS; iter++) {
        FILE *fp = fopen(testpath, "wb");
        u64 start, end;
        if (!fp) {
            snprintf(buf, sizeof(buf), "Cannot create test file on %s", device_name);
            ui_draw_err(buf);
            free(buffer);
            return;
        }
        start = gettime();
        for (i = 0; i < blocks; i++)
            fwrite(buffer, 1, TEST_BLOCK_SIZE, fp);
        fflush(fp);
        fclose(fp);
        end = gettime();
        write_total_ticks += (end - start);
    }

    {
        float write_time_ms = (float)ticks_to_millisecs(write_total_ticks / TEST_ITERATIONS);
        write_speed_kbs = (write_time_ms > 0)
            ? (float)(TEST_FILE_SIZE / 1024) * 1000.0f / write_time_ms
            : 0.0f;
    }

    /* Read speed */
    ui_printf("   " UI_WHITE "Running read speed test...\n" UI_RESET);

    for (iter = 0; iter < TEST_ITERATIONS; iter++) {
        FILE *fp = fopen(testpath, "rb");
        u64 start, end;
        if (!fp) { ui_draw_err("Cannot open test file for reading"); break; }
        start = gettime();
        for (i = 0; i < blocks; i++)
            fread(buffer, 1, TEST_BLOCK_SIZE, fp);
        fclose(fp);
        end = gettime();
        read_total_ticks += (end - start);
    }

    {
        float read_time_ms = (float)ticks_to_millisecs(read_total_ticks / TEST_ITERATIONS);
        read_speed_kbs = (read_time_ms > 0)
            ? (float)(TEST_FILE_SIZE / 1024) * 1000.0f / read_time_ms
            : 0.0f;
    }

    remove(testpath);
    free(buffer);

    /* Results */
    write_color = (write_speed_kbs > SPEED_GOOD_KB) ? UI_BGREEN :
                  (write_speed_kbs > SPEED_OK_KB) ? UI_BYELLOW : UI_BRED;
    read_color  = (read_speed_kbs > SPEED_GOOD_KB) ? UI_BGREEN :
                  (read_speed_kbs > SPEED_OK_KB) ? UI_BYELLOW : UI_BRED;

    ui_printf("\n");
    snprintf(buf, sizeof(buf), "%.1f KB/s (%.2f MB/s)",
             write_speed_kbs, write_speed_kbs / 1024.0f);
    ui_draw_kv_color("Write Speed", write_color, buf);

    snprintf(buf, sizeof(buf), "%.1f KB/s (%.2f MB/s)",
             read_speed_kbs, read_speed_kbs / 1024.0f);
    ui_draw_kv_color("Read Speed", read_color, buf);

    /* Rating */
    if (write_speed_kbs > SPEED_GOOD_KB && read_speed_kbs > SPEED_GOOD_KB) {
        rating = "Excellent";
        snprintf(buf, sizeof(buf), "Speed Rating: %s", rating);
        ui_draw_ok(buf);
    } else if (write_speed_kbs > SPEED_OK_KB && read_speed_kbs > SPEED_OK_KB) {
        rating = "Acceptable";
        snprintf(buf, sizeof(buf), "Speed Rating: %s", rating);
        ui_draw_warn(buf);
    } else {
        rating = "Slow - may cause issues with game loading";
        snprintf(buf, sizeof(buf), "Speed Rating: %s", rating);
        ui_draw_err(buf);
    }
}

/*---------------------------------------------------------------------------*/
void run_storage_test(void) {
    int rpos = 0;
    bool sd_present, usb_present;

    memset(s_report, 0, sizeof(s_report));
    rpos = snprintf(s_report, sizeof(s_report), "=== STORAGE SPEED TEST ===\n");

    sd_present  = check_device_present("sd:/");
    usb_present = check_device_present("usb:/");

    /* SD Card */
    ui_draw_section("SD Card");

    if (sd_present) {
        get_device_info("SD Card", "sd:/");
        run_benchmark("SD Card", "sd:");
        rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
            "SD Card: Detected, benchmark completed\n");
    } else {
        ui_draw_warn("SD Card not detected");
        ui_draw_info("Insert an SD card and restart to test");
        rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
            "SD Card: Not detected\n");
    }

    /* USB */
    ui_draw_section("USB Storage");

    if (usb_present) {
        get_device_info("USB Storage", "usb:/");
        run_benchmark("USB Storage", "usb:");
        rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
            "USB Storage: Detected, benchmark completed\n");
    } else {
        ui_printf("   " UI_WHITE "USB not detected (normal if none is connected)\n" UI_RESET);
        ui_draw_info("USB must be in the port closest to the edge");
        rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
            "USB Storage: Not detected\n");
    }

    /* Tips */
    ui_draw_section("Tips");
    ui_draw_info("Use the bottom USB port (closest to edge) for best results");
    ui_draw_info("USB 2.0 drives recommended; USB 3.0 works at 2.0 speeds");
    ui_draw_info("SDHC cards (Class 10 / UHS-I) give best SD performance");
    ui_draw_info("Format USB as FAT32 (32KB clusters) or WBFS for games");

    rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos, "\n");

    ui_printf("\n");
    ui_draw_ok("Storage test complete");
}

/*---------------------------------------------------------------------------*/
void get_storage_test_report(char *buf, int bufsize) {
    strncpy(buf, s_report, bufsize - 1);
    buf[bufsize - 1] = '\0';
}
