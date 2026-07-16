// network_test.c
// tests the WiFi module, grabs IP config, checks internet connectivity,
// and scans for nearby access points.
//
// the order here matters a lot. net_init() and WD_Init() both need the
// WiFi hardware and they will fight each other if you run them at the same time.
// the flow is: net_init -> get IP -> net_deinit -> WD_Init -> card info + AP scan
// -> WD_Deinit -> retry net_init if first attempt failed.
// it's annoying but it's the only way to get both pieces of info reliably.

#include <errno.h>
#include <gccore.h>
#include <network.h>
#include <ogc/lwp.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/wd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "network_test.h"
#include "ui_common.h"

#define MAX_SCAN_APS  32
#define SCAN_BUF_SIZE 4096

// these may or may not be defined in your libogc version
#ifndef CAPAB_SECURED_FLAG
#define CAPAB_SECURED_FLAG 0x0010
#endif
#ifndef IEID_SECURITY
#define IEID_SECURITY 48
#endif
#ifndef AOSSAPScan
#define AOSSAPScan 3  // scan-only WD init mode, doesn't need NCD lock
#endif

static char s_report[8192];
static bool s_wifi_ok      = false;
static bool s_wd_ok        = false;
static bool s_ip_ok        = false;
static char s_ip_str[32]   = "N/A";
static bool s_test_done    = false;
static bool s_wdinfo_valid = false;

// these buffers go to IOS over IPC so they need to be 32-byte aligned.
// the guard buffers on either side are there to catch any overruns.
// probably paranoid but i've seen weird memory corruption from IOS calls before.
static u8     s_wd_guard1[64]            __attribute__((aligned(32), unused));
static WDInfo s_wdinfo                   __attribute__((aligned(32)));
static u8     s_wd_guard2[64]            __attribute__((aligned(32), unused));
static u8     s_scan_buf[SCAN_BUF_SIZE]  __attribute__((aligned(32)));

// net_init can block for several seconds, so we run it on a thread.
// otherwise the screen just freezes and people think it crashed.
static lwp_t          s_net_thread;
static u8             s_net_stack[8192]  __attribute__((aligned(32)));
static s32            s_net_ret;
static volatile bool  s_net_done;

static void *net_init_thread(void *arg) {
    (void)arg;
    s_net_ret  = net_init();
    s_net_done = true;
    return NULL;
}


bool has_network_test_run(void) { return s_test_done; }


static void ip_to_str(u32 ip, char *buf, size_t sz) {
    snprintf(buf, sz, "%d.%d.%d.%d",
             (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
             (ip >>  8) & 0xFF,  ip        & 0xFF);
}

static void mac_to_str(const u8 *mac, char *buf, size_t sz) {
    snprintf(buf, sz, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}


static const char *security_str(BSSDescriptor *bss) {
    if (!(bss->Capabilities & CAPAB_SECURED_FLAG)) return "Open";
    if (WD_GetIELength(bss, IEID_SECURITY) > 0)    return "WPA2";
    return "WEP/WPA";
}

static const char *signal_str(u8 level) {
    if (level == 0) return "Weak  ";
    if (level == 1) return "Fair  ";
    if (level == 2) return "Good  ";
    return "Strong";
}


// tries a TCP connect to a known host. measures how long it takes.
// we use this to check if the internet is actually reachable, not just that
// we got an IP (DHCP can hand you an address even with no route to the internet)
static bool test_tcp(const char *label, u32 host_ip, u16 port) {
    s32 sock = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        ui_draw_err("Socket creation failed - this shouldn't happen");
        return false;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = htonl(host_ip);

    u64 t0 = gettime();
    s32 ret = net_connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    float ms = (float)ticks_to_millisecs(gettime() - t0);
    net_close(sock);

    char buf[128];
    if (ret >= 0) {
        snprintf(buf, sizeof(buf), "%s: Connected (%.0f ms)", label, ms);
        ui_draw_ok(buf);
        return true;
    } else {
        snprintf(buf, sizeof(buf), "%s: Failed (error %d)", label, ret);
        ui_draw_err(buf);
        return false;
    }
}


static void wait_frames(int n) {
    int i;
    for (i = 0; i < n; i++) VIDEO_WaitVSync();
}


// figures out how long each BSS descriptor entry is in the scan buffer.
// the length field is in 2-byte units when nonzero, otherwise we calculate it.
static u16 bss_entry_len(BSSDescriptor *bss) {
    if (bss->length != 0)
        return (u16)(bss->length * 2);
    // fallback: IEs_length + fixed header size, rounded up to 2
    u16 base = (u16)(bss->IEs_length + 0x3E);
    return (base % 2 == 0) ? base : base + 1;
}


// parses the scan results buffer and prints each AP.
// the buffer format varies by IOS version which is delightful.
// we try the "2-byte count then descriptors" format first, fall back to
// walking by bss->length if that turns up nothing.
static int parse_and_display_aps(int *rpos_ptr, u8 *buf, s32 scan_ret) {
    int rpos = *rpos_ptr;
    int count = 0;
    u8 *ptr = buf;
    u8 *end = buf + SCAN_BUF_SIZE;

    rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                     "\n--- Nearby Access Points ---\n");

    if (scan_ret < 0) {
        rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                         "  AP scan failed (error %d)\n", (int)scan_ret);
        *rpos_ptr = rpos;
        return -1;
    }

    // format A: [count_hi, count_lo] followed by BSSDescriptor array
    if (ptr + 2 <= end) {
        u16 n = (u16)((ptr[0] << 8) | ptr[1]);
        ptr += 2;

        if (n > 0 && n <= 64) {
            int i;
            for (i = 0; i < n && ptr < (end - sizeof(BSSDescriptor)) && count < MAX_SCAN_APS; i++) {
                BSSDescriptor *bss = (BSSDescriptor *)ptr;
                u16 entry_len;

                if (bss->SSIDLength > 32) {
                    entry_len = sizeof(BSSDescriptor);
                } else {
                    entry_len = bss_entry_len(bss);
                    if (entry_len < sizeof(BSSDescriptor))
                        entry_len = sizeof(BSSDescriptor);
                }

                if (ptr + entry_len > end) break;

                // skip zero BSSIDs (padding / uninitialized entries)
                if (!bss->BSSID[0] && !bss->BSSID[1] && !bss->BSSID[2] &&
                    !bss->BSSID[3] && !bss->BSSID[4] && !bss->BSSID[5]) {
                    ptr += entry_len;
                    continue;
                }

                char ssid[33], bssid_str[20], line[128];
                memset(ssid, 0, sizeof(ssid));
                if (bss->SSIDLength > 0 && bss->SSIDLength <= 32)
                    memcpy(ssid, bss->SSID, bss->SSIDLength);
                else
                    strcpy(ssid, "(Hidden)");

                mac_to_str(bss->BSSID, bssid_str, sizeof(bssid_str));
                u8 sig = WD_GetRadioLevel(bss);

                snprintf(line, sizeof(line), "%-24s Ch:%-2d  Sig:%s  %s",
                         ssid, bss->channel, signal_str(sig), security_str(bss));

                if (sig >= 2) ui_draw_ok(line);
                else if (sig == 1) ui_draw_warn(line);
                else ui_draw_err(line);

                rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                                 "  %s  BSSID:%s  Ch:%d  Signal:%s  %s\n",
                                 ssid, bssid_str, bss->channel, signal_str(sig), security_str(bss));
                count++;
                ptr += entry_len;
            }
        }
    }

    // format B fallback: no leading count, stride from bss->length
    if (count == 0) {
        ptr = buf;
        while (ptr < (end - sizeof(BSSDescriptor)) && count < MAX_SCAN_APS) {
            BSSDescriptor *bss = (BSSDescriptor *)ptr;
            if (bss->length == 0 || bss->length < sizeof(BSSDescriptor) || bss->SSIDLength > 32)
                break;

            if (!bss->BSSID[0] && !bss->BSSID[1] && !bss->BSSID[2] &&
                !bss->BSSID[3] && !bss->BSSID[4] && !bss->BSSID[5]) {
                ptr += bss->length;
                continue;
            }

            char ssid[33], bssid_str[20], line[128];
            memset(ssid, 0, sizeof(ssid));
            if (bss->SSIDLength > 0) memcpy(ssid, bss->SSID, bss->SSIDLength);
            else strcpy(ssid, "(Hidden)");

            mac_to_str(bss->BSSID, bssid_str, sizeof(bssid_str));
            u8 sig = WD_GetRadioLevel(bss);

            snprintf(line, sizeof(line), "%-24s Ch:%-2d  Sig:%s  %s",
                     ssid, bss->channel, signal_str(sig), security_str(bss));

            if (sig >= 2) ui_draw_ok(line);
            else if (sig == 1) ui_draw_warn(line);
            else ui_draw_err(line);

            rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                             "  %s  BSSID:%s  Ch:%d  Signal:%s  %s\n",
                             ssid, bssid_str, bss->channel, signal_str(sig), security_str(bss));
            count++;
            ptr += bss->length;
        }
    }

    if (count == 0) {
        ui_draw_warn("No access points found");
        rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos, "  (none found)\n");
    } else {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "Found %d access point(s)", count);
        ui_draw_ok(tmp);
    }

    *rpos_ptr = rpos;
    return count;
}


// runs IP acquisition and connectivity tests.
// split into its own function so we can call it from two places
// (initial attempt and the retry after WD releases the radio)
static bool run_connectivity(void) {
    u32 ip = net_gethostip();
    if (ip == 0) {
        ui_draw_section("IP Configuration");
        ui_draw_err("No IP address - DHCP failed or not connected");
        return false;
    }

    u8 a = (ip >> 24) & 0xFF, b = (ip >> 16) & 0xFF;
    s_ip_ok = true;
    ip_to_str(ip, s_ip_str, sizeof(s_ip_str));

    ui_draw_section("IP Configuration");
    ui_draw_kv("IP Address", s_ip_str);
    ui_draw_kv("Config", "DHCP");

    if      (a == 192 && b == 168)              ui_draw_ok("Private range 192.168.x.x");
    else if (a == 10)                           ui_draw_ok("Private range 10.x.x.x");
    else if (a == 172 && b >= 16 && b <= 31)    ui_draw_ok("Private range 172.16-31.x.x");
    else if (a == 169 && b == 254)              ui_draw_warn("169.254.x.x - link-local, DHCP probably failed");

    ui_draw_section("Connectivity Tests");

    // test against Google DNS port 53 and Cloudflare HTTP port 80.
    // these are pretty reliable targets - if both fail the internet is definitely not working.
    bool dns_ok  = test_tcp("Google DNS (8.8.8.8:53)",  0x08080808, 53);
    bool http_ok = test_tcp("Cloudflare (1.1.1.1:80)",  0x01010101, 80);
    ui_printf("\n");

    if (dns_ok && http_ok) {
        ui_draw_ok("Internet: FULL connectivity");
        ui_draw_info("Wiimmfi, WiiLink, RiiConnect24 should all work");
    } else if (dns_ok || http_ok) {
        ui_draw_warn("Internet: PARTIAL - some things may not work");
    } else {
        ui_draw_err("Internet: NONE - WiFi connected but no internet");
        ui_draw_warn("Check your router or ISP");
    }

    return dns_ok || http_ok;
}


// spawns the net_init thread and waits for it to finish.
// returns whatever net_init returned.
static s32 do_net_init(void) {
    s_net_done = false;
    LWP_CreateThread(&s_net_thread, net_init_thread, NULL,
                     s_net_stack, sizeof(s_net_stack), 64);
    while (!s_net_done) VIDEO_WaitVSync();
    return s_net_ret;
}


void run_network_test(void) {
    int rpos = 0;
    s32 last_err = 0;

    memset(s_report, 0, sizeof(s_report));
    memset(&s_wdinfo, 0, sizeof(s_wdinfo));
    memset(s_scan_buf, 0, sizeof(s_scan_buf));
    s_wifi_ok = s_ip_ok = s_wd_ok = s_wdinfo_valid = false;
    strcpy(s_ip_str, "N/A");

    // write the header with placeholder values that we'll patch in later
    rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                     "=== NETWORK TEST ===\n"
                     "WiiMedic Version:    v%-14s\n"
                     "WiFi Module:         Searching...   \n"
                     "IP Address:          Searching...   \n\n",
                     WIIMEDIC_VERSION);

    // clean slate for the network stack before we try anything
    net_deinit();
    wait_frames(30);

    s32 ret = do_net_init();

    if (ret < 0) {
        last_err = ret;
        char msg[128];
        snprintf(msg, sizeof(msg), "WiFi init failed (error %d)", ret);
        ui_draw_err(msg);
        ui_printf("\n");

        // try to give a useful hint based on the error code
        switch (ret) {
            case -EAGAIN:
                ui_draw_warn("Network stack is busy, try again in a moment");
                break;
            case -6:
                ui_draw_warn("No wireless network configured");
                ui_draw_info("Set up WiFi in Wii System Settings first");
                break;
            case -24:
                ui_draw_warn("Not connected (error -24)");
                ui_draw_info("Go to Wii Settings -> Internet -> Connection Settings");
                break;
            case -116:
                ui_draw_warn("Connection timed out (error -116)");
                ui_draw_info("The Wii only supports 2.4GHz networks with WPA2.");
                ui_draw_info("WPA3 and 5GHz are not supported at all.");
                break;
            default:
                ui_draw_warn("Unknown error - WiFi may not be set up");
                break;
        }
        net_deinit();
    } else {
        s_wifi_ok = true;
        ui_draw_ok("WiFi module initialized");
        run_connectivity();
        net_deinit();
    }

    // --- WiFi Card Info + AP Scan ---
    // WD needs the radio free from net_init before it can do anything

    wait_frames(60);
    ui_draw_section("WiFi Card Info");
    ui_draw_info("Reading card info and scanning for APs...");
    ui_printf("\n");

    {
        char mac_str[32];
        char chan_buf[256];
        bool wd_ready = false;

        // try mode 0 first (normal), fall back to AOSSAPScan if that fails
        if (WD_Init(0) == 0)          wd_ready = true;
        if (!wd_ready && WD_Init(AOSSAPScan) == 0) wd_ready = true;

        if (!wd_ready) {
            ui_draw_err("WiFi driver init failed (WD_Init returned error)");
            rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                             "WiFi Driver: FAILED\n");
        } else {
            s_wd_ok = true;
            wait_frames(30);

            // grab card info
            memset(&s_wdinfo, 0, sizeof(s_wdinfo));
            if (WD_GetInfo(&s_wdinfo) == 0) {
                int ci, ch_pos = 0;
                bool mac_ok = false;
                int i;

                for (i = 0; i < 6; i++) {
                    if (s_wdinfo.MAC[i] != 0 && s_wdinfo.MAC[i] != 0xFF) {
                        mac_ok = true; break;
                    }
                }

                if (mac_ok && s_wdinfo.channel >= 1 && s_wdinfo.channel <= 14) {
                    s_wdinfo_valid = true;
                    mac_to_str(s_wdinfo.MAC, mac_str, sizeof(mac_str));
                    ui_draw_kv("MAC Address", mac_str);

                    // sanitize the firmware version string - IOS sometimes
                    // packs garbage bytes in here and it corrupts the display
                    s_wdinfo.version[sizeof(s_wdinfo.version) - 1] = '\0';
                    for (ci = 0; ci < (int)sizeof(s_wdinfo.version); ci++) {
                        u8 c = s_wdinfo.version[ci];
                        if (c == 0) break;
                        if (c < 0x20 || c > 0x7E) s_wdinfo.version[ci] = '?';
                    }
                    ui_draw_kv("Firmware", (const char *)s_wdinfo.version);

                    {
                        char cc[8];
                        u8 c0 = s_wdinfo.CountryCode[0], c1 = s_wdinfo.CountryCode[1];
                        if (c0 >= 0x20 && c0 <= 0x7E && c1 >= 0x20 && c1 <= 0x7E)
                            snprintf(cc, sizeof(cc), "%c%c", c0, c1);
                        else
                            snprintf(cc, sizeof(cc), "??");
                        ui_draw_kv("Country Code", cc);
                    }

                    {
                        char ch_str[8];
                        snprintf(ch_str, sizeof(ch_str), "%d", s_wdinfo.channel);
                        ui_draw_kv("Current Channel", ch_str);
                    }

                    chan_buf[0] = '\0';
                    for (ci = 1; ci <= 14; ci++) {
                        if (s_wdinfo.EnableChannelsMask & (1 << (ci - 1))) {
                            if (ch_pos > 0)
                                ch_pos += snprintf(chan_buf + ch_pos, sizeof(chan_buf) - ch_pos, ", ");
                            ch_pos += snprintf(chan_buf + ch_pos, sizeof(chan_buf) - ch_pos, "%d", ci);
                        }
                    }
                    if (ch_pos > 0) ui_draw_kv("Enabled Channels", chan_buf);

                    ui_draw_ok("Card info read successfully");

                    rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                                     "MAC Address:         %s\n"
                                     "Firmware:            %s\n"
                                     "Current Channel:     %d\n"
                                     "Enabled Channels:    %s\n",
                                     mac_str, (const char *)s_wdinfo.version,
                                     s_wdinfo.channel, chan_buf);
                } else {
                    ui_draw_warn("Card info came back invalid (bad MAC or channel)");
                    rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                                     "WiFi Card Info: INVALID\n");
                }
            } else {
                ui_draw_err("WD_GetInfo failed");
                rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                                 "WiFi Card Info: FAILED\n");
            }

            // AP scan - some IOS versions specifically need AOSSAPScan mode for this
            ui_draw_section("Nearby Access Points");
            ui_draw_info("Scanning...");

            WD_Deinit();
            wait_frames(30);
            WD_Init(AOSSAPScan);

            {
                ScanParameters sp;
                WD_SetDefaultScanParameters(&sp);
                sp.MaxChannelTime  = 400;
                sp.ChannelBitmap   = 0x3FFF;  // scan all 14 channels
                memset(s_scan_buf, 0, sizeof(s_scan_buf));

                s32 scan_ret = WD_ScanOnce(&sp, s_scan_buf, sizeof(s_scan_buf));

                // retry once if the results look empty
                if (scan_ret >= 0 && s_scan_buf[0] == 0 && s_scan_buf[1] == 0) {
                    wait_frames(45);
                    scan_ret = WD_ScanOnce(&sp, s_scan_buf, sizeof(s_scan_buf));
                }

                parse_and_display_aps(&rpos, s_scan_buf, scan_ret);
            }

            // release the driver so net_init can use the hardware again
            WD_Deinit();
            wait_frames(60);
        }
    }

    // if the first net_init failed, try again now that WD has released the radio.
    // error -24 and -116 often succeed on retry once the hardware is fully free.
    if (!s_wifi_ok) {
        ui_draw_section("Network Connectivity (Retry)");
        ui_draw_info("Trying again after WD release...");
        wait_frames(60);
        net_deinit();
        wait_frames(30);

        s32 ret2 = do_net_init();
        if (ret2 >= 0) {
            s_wifi_ok = true;
            ui_draw_ok("Connected on retry!");
            run_connectivity();
            net_deinit();
        } else {
            last_err = ret2;
            char msg[80];
            snprintf(msg, sizeof(msg), "Retry also failed (error %d)", ret2);
            ui_draw_warn(msg);

            if (ret2 == -24)
                ui_draw_info("Configure WiFi in Wii Settings -> Internet first");
            else if (ret2 == -116)
                ui_draw_info("Error -116 = timeout. Router may be rejecting connection.");

            net_deinit();
        }
    }

    // build report connectivity section
    if (s_wifi_ok) {
        rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                         "\n=== NETWORK CONNECTIVITY ===\nWiFi Status: Connected\n"
                         "IP Address: %s\n", s_ip_str);
    } else {
        rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                         "\n=== NETWORK CONNECTIVITY ===\nWiFi Status: FAILED (error %d)\n",
                         last_err);
        if (last_err == -24)
            rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                             "  (error -24 = no connection configured in Wii Settings)\n");
        else if (last_err == -116)
            rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                             "  (error -116 = connection timed out)\n");
    }

    ui_draw_section("WiFi Notes");
    ui_draw_info("Wii supports 802.11b/g on 2.4GHz only - no 5GHz, no 6GHz");
    ui_draw_info("WPA2-PSK (AES) is the way to go for security");
    ui_draw_info("WPA3, WPA Enterprise, and captive portals won't work");
    ui_draw_info("Wiimmfi needs ports 28910 and 29900-29901 open on your router");

    // patch the header placeholders now that we have final status
    {
        char *p = strstr(s_report, "Searching...   ");
        if (p) {
            char tmp[16];
            snprintf(tmp, sizeof(tmp), "%-14s", s_wd_ok ? "Working" : "Failed");
            memcpy(p, tmp, 14);

            p = strstr(p + 14, "Searching...   ");
            if (p) {
                snprintf(tmp, sizeof(tmp), "%-14s", s_ip_str);
                memcpy(p, tmp, 14);
            }
        }
    }

    rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos, "\n");

    ui_printf("\n");
    ui_draw_ok("Network test complete");
    s_test_done = true;
}


void get_network_test_report(char *buf, int bufsize) {
    strncpy(buf, s_report, bufsize - 1);
    buf[bufsize - 1] = '\0';
}
