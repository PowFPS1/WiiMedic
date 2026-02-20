/*
 * WiiMedic - network_test.c
 * Tests WiFi module, connection status, IP configuration,
 * WiFi card info, and nearby AP scanning (libogc 3.0.0+)
 *
 * Order: connectivity first (net_init -> IP -> connection tests -> net_deinit),
 * then WiFi card info + AP scan (WD_Init in scan mode after network released).
 * This lets the driver be free for WD so AP scan can work without NCD lock.
 *
 * WiFi card info and AP scan implementation: thanks to Abdelali221 for help
 * with the network code.
 */

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

/* Max APs to display from scan results */
#define MAX_SCAN_APS 32
/* Buffer for raw scan data (BSSDescriptors + IEs) */
#define SCAN_BUF_SIZE 4096

#ifndef CAPAB_SECURED_FLAG
#define CAPAB_SECURED_FLAG 0x0010
#endif
#ifndef IEID_SECURITY
#define IEID_SECURITY 48
#endif
/* Scan-only mode for WD_Init; may work without NCD lock (e.g. libogc / IOS) */
#ifndef AOSSAPScan
#define AOSSAPScan 3
#endif

static char s_report[8192];
static bool s_wifi_working = false;
static bool s_wifi_driver_ok = false; /* true if WD_Init + card info worked */
static bool s_ip_obtained = false;
static char s_ip_str[32] = "N/A";
static bool s_test_done = false;
static bool s_wdinfo_valid = false;

/* Aligned buffers for IOS/Hardware IPC - Added padding to prevent aliasing */
static u8 s_wd_guard1[64] __attribute__((aligned(32), unused));
static WDInfo s_wdinfo __attribute__((aligned(32)));
static u8 s_wd_guard2[64] __attribute__((aligned(32), unused));
static u8 s_scan_buf[SCAN_BUF_SIZE] __attribute__((aligned(32)));

static lwp_t s_net_thread;
static u8 s_net_stack[8192] __attribute__((aligned(32)));
static s32 s_net_ret;
static volatile bool s_net_done;

static void *net_init_thread_func(void *arg) {
  s_net_ret = net_init();
  s_net_done = true;
  return NULL;
}

/*---------------------------------------------------------------------------*/
bool has_network_test_run(void) { return s_test_done; }

/*---------------------------------------------------------------------------*/
static void ip_to_str(u32 ip, char *buf) {
  sprintf(buf, "%d.%d.%d.%d", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
          (ip >> 8) & 0xFF, ip & 0xFF);
}

/*---------------------------------------------------------------------------*/
static void mac_to_str(const u8 *mac, char *buf) {
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3],
          mac[4], mac[5]);
}

/*---------------------------------------------------------------------------*/
static const char *get_security_str(BSSDescriptor *bss) {
  if (!(bss->Capabilities & CAPAB_SECURED_FLAG))
    return "Open";
  if (WD_GetIELength(bss, IEID_SECURITY) > 0)
    return "WPA2";
  return "WEP/WPA";
}

/*---------------------------------------------------------------------------*/
static const char *get_signal_str(u8 level) {
  if (level == 0)
    return "Weak  ";
  if (level == 1)
    return "Fair  ";
  if (level == 2)
    return "Good  ";
  return "Strong";
}

/*---------------------------------------------------------------------------*/
static bool test_tcp_connection(const char *host_desc, u32 host_ip, u16 port) {
  s32 sock = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  struct sockaddr_in addr;
  u64 start, end;
  float latency_ms;
  s32 ret;
  char buf[128];

  if (sock < 0) {
    ui_draw_err("Socket creation failed");
    return false;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(host_ip);

  start = gettime();
  ret = net_connect(sock, (struct sockaddr *)&addr, sizeof(addr));
  end = gettime();

  latency_ms = (float)ticks_to_millisecs(end - start);
  net_close(sock);

  if (ret >= 0) {
    snprintf(buf, sizeof(buf), "%s: Connected (%.0f ms)", host_desc,
             latency_ms);
    ui_draw_ok(buf);
    return true;
  } else {
    snprintf(buf, sizeof(buf), "%s: Connection failed (error %d)", host_desc,
             ret);
    ui_draw_err(buf);
    return false;
  }
}

/*---------------------------------------------------------------------------*/
static void delay_vsyncs(int count) {
  int i;
  for (i = 0; i < count; i++)
    VIDEO_WaitVSync();
}

/*---------------------------------------------------------------------------*/
/* Compute BSS entry length. Some buffers use descriptor length + IEs. */
static u16 bss_entry_len(BSSDescriptor *bss) {
  if (bss->length != 0)
    return (u16)(bss->length * 2);
  /* length 0: use IEs_length + fixed descriptor size, align to 2 */
  {
    u16 base = (u16)(bss->IEs_length + 0x3E);
    return (base % 2 == 0) ? base : base + 1;
  }
}

/*---------------------------------------------------------------------------*/
/*
 * Parse scan buffer and report APs. Tries format: 2-byte big-endian count
 * then BSSDescriptor list (entry stride from bss_entry_len). Falls back to
 * walking by bss->length if count format yields no valid APs.
 */
static int do_ap_scan(int *rpos_ptr, u8 *scan_buf, s32 scan_ret) {
  int rpos = *rpos_ptr;
  int scan_count = 0;
  u8 *ptr = scan_buf;
  u8 *end = scan_buf + SCAN_BUF_SIZE;

  rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                   "\n--- Nearby Access Points ---\n");

  if (scan_ret < 0) {
    rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                     "  AP scan failed (error %d)\n", (int)scan_ret);
    *rpos_ptr = rpos;
    return -1;
  }

  /* Try format: [count_hi, count_lo] then BSSDescriptor entries */
  if (ptr + 2 <= end) {
    u16 count = (u16)((ptr[0] << 8) | ptr[1]);
    ptr += 2;

    if (count > 0 && count <= 64) {
      int i;
      for (i = 0; i < count && ptr < (end - sizeof(BSSDescriptor)); i++) {
        BSSDescriptor *bss = (BSSDescriptor *)ptr;
        u16 entry_len;
        char ssid[33];
        char bssid_str[20];
        char line[128];
        u8 signal;

        if (bss->SSIDLength > 32) {
          entry_len = sizeof(BSSDescriptor);
        } else {
          entry_len = bss_entry_len(bss);
          if (entry_len < sizeof(BSSDescriptor))
            entry_len = sizeof(BSSDescriptor);
        }

        if (ptr + entry_len > end)
          break;

        /* Skip all-zero BSSID */
        if (bss->BSSID[0] == 0 && bss->BSSID[1] == 0 && bss->BSSID[2] == 0 &&
            bss->BSSID[3] == 0 && bss->BSSID[4] == 0 && bss->BSSID[5] == 0) {
          ptr += entry_len;
          continue;
        }

        memset(ssid, 0, sizeof(ssid));
        if (bss->SSIDLength > 0 && bss->SSIDLength <= 32)
          memcpy(ssid, bss->SSID, bss->SSIDLength);
        else
          strcpy(ssid, "(Hidden)");

        mac_to_str(bss->BSSID, bssid_str);
        signal = WD_GetRadioLevel(bss);

        snprintf(line, sizeof(line), "%-24s Ch:%-2d  Sig:%s  %s", ssid,
                 bss->channel, get_signal_str(signal), get_security_str(bss));

        if (signal >= 2)
          ui_draw_ok(line);
        else if (signal == 1)
          ui_draw_warn(line);
        else
          ui_draw_err(line);

        rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                         "  %s  BSSID:%s  Ch:%d  Signal:%s  %s\n", ssid,
                         bssid_str, bss->channel, get_signal_str(signal),
                         get_security_str(bss));

        scan_count++;
        if (scan_count >= MAX_SCAN_APS)
          break;
        ptr += entry_len;
      }
    }
  }

  /* Fallback: no leading count, stride = bss->length */
  if (scan_count == 0) {
    ptr = scan_buf;
    while (ptr < (end - sizeof(BSSDescriptor)) && scan_count < MAX_SCAN_APS) {
      BSSDescriptor *bss = (BSSDescriptor *)ptr;
      u16 entry_len;

      if (bss->length == 0 || bss->length < sizeof(BSSDescriptor) ||
          bss->SSIDLength > 32)
        break;

      if (bss->BSSID[0] == 0 && bss->BSSID[1] == 0 && bss->BSSID[2] == 0 &&
          bss->BSSID[3] == 0 && bss->BSSID[4] == 0 && bss->BSSID[5] == 0) {
        ptr += bss->length;
        continue;
      }

      entry_len = bss->length;

      {
        char ssid[33];
        char bssid_str[20];
        char line[128];
        u8 signal;

        memset(ssid, 0, sizeof(ssid));
        if (bss->SSIDLength > 0)
          memcpy(ssid, bss->SSID, bss->SSIDLength);
        else
          strcpy(ssid, "(Hidden)");

        mac_to_str(bss->BSSID, bssid_str);
        signal = WD_GetRadioLevel(bss);

        snprintf(line, sizeof(line), "%-24s Ch:%-2d  Sig:%s  %s", ssid,
                 bss->channel, get_signal_str(signal), get_security_str(bss));

        if (signal >= 2)
          ui_draw_ok(line);
        else if (signal == 1)
          ui_draw_warn(line);
        else
          ui_draw_err(line);

        rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                         "  %s  BSSID:%s  Ch:%d  Signal:%s  %s\n", ssid,
                         bssid_str, bss->channel, get_signal_str(signal),
                         get_security_str(bss));
      }

      scan_count++;
      ptr += entry_len;
    }
  }

  if (scan_count == 0) {
    ui_draw_warn("No access points found");
    rpos +=
        snprintf(s_report + rpos, sizeof(s_report) - rpos, "  (none found)\n");
  } else {
    char cnt[64];
    snprintf(cnt, sizeof(cnt), "Found %d access point(s)", scan_count);
    ui_draw_ok(cnt);
  }

  *rpos_ptr = rpos;
  return scan_count;
}

/*---------------------------------------------------------------------------*/
void run_network_test(void) {
  int rpos = 0;
  s32 ret;
  s32 connectivity_ret = 0; /* used for report if connectivity never succeeds */

  memset(s_report, 0, sizeof(s_report));
  memset(&s_wdinfo, 0, sizeof(s_wdinfo));
  memset(s_scan_buf, 0, sizeof(s_scan_buf));
  s_wifi_working = false;
  s_ip_obtained = false;
  s_wdinfo_valid = false;
  strcpy(s_ip_str, "N/A");

  /* Write header with fixed-width placeholders (patched later with memcpy) */
  rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                   "=== NETWORK TEST ===\n"
                   "Net Build:           v%-14s\n"
                   "WiFi Module:         Searching...   \n"
                   "IP Address:          Searching...   \n\n",
                   WIIMEDIC_VERSION);

  /* Clean state for network module */
  net_deinit();
  delay_vsyncs(30);

  s_net_done = false;
  LWP_CreateThread(&s_net_thread, net_init_thread_func, NULL, s_net_stack,
                   sizeof(s_net_stack), 64);
  while (!s_net_done) {
    VIDEO_WaitVSync();
  }
  ret = s_net_ret;

  if (ret < 0) {
    connectivity_ret = ret;
    char msg[128];
    snprintf(msg, sizeof(msg), "Network initialization failed (error %d)", ret);
    ui_draw_err(msg);
    ui_printf("\n");
    switch (ret) {
    case -EAGAIN:
      ui_draw_warn("Network module busy - try again");
      break;
    case -6:
      ui_draw_warn("No wireless network configured");
      ui_draw_info("Configure WiFi in Wii System Settings first");
      break;
    case -24:
      ui_draw_warn("No connection (error -24)");
      ui_draw_info("Wii Settings -> Internet -> Connection Settings");
      break;
    case -116:
      ui_draw_warn("Connection failed (error -116)");
      ui_draw_info("Timeout or no response from router.");
      ui_draw_info("Hotspots may require 2.4GHz and WPA2.");
      break;
    default:
      ui_draw_warn("WiFi module status unknown");
      break;
    }
    net_deinit();
    /* Report written later after possible retry */
  } else {
    s_wifi_working = true;
    ui_draw_ok("WiFi module initialized successfully");

    ui_draw_section("IP Configuration");
    {
      u32 ip = net_gethostip();
      if (ip != 0) {
        u8 first_octet, second_octet;
        s_ip_obtained = true;
        ip_to_str(ip, s_ip_str);
        ui_draw_kv("IP Address", s_ip_str);
        ui_draw_kv("Config Method", "Obtained via DHCP");
        first_octet = (ip >> 24) & 0xFF;
        second_octet = (ip >> 16) & 0xFF;
        if (first_octet == 192 && second_octet == 168)
          ui_draw_ok("Valid private IP range (192.168.x.x)");
        else if (first_octet == 10)
          ui_draw_ok("Valid private IP range (10.x.x.x)");
        else if (first_octet == 172 && second_octet >= 16 && second_octet <= 31)
          ui_draw_ok("Valid private IP range (172.16-31.x.x)");
        else if (first_octet == 169 && second_octet == 254)
          ui_draw_warn("Link-local IP (169.254.x.x) - DHCP may have failed");
      } else {
        ui_draw_err("No IP address obtained");
        ui_draw_warn("WiFi connected but DHCP failed");
      }
    }

    ui_draw_section("Connection Tests");
    if (s_ip_obtained) {
      bool dns_ok =
          test_tcp_connection("Google DNS (8.8.8.8:53)", 0x08080808, 53);
      bool http_ok =
          test_tcp_connection("HTTP Test (1.1.1.1:80)", 0x01010101, 80);
      ui_printf("\n");
      if (dns_ok && http_ok) {
        ui_draw_ok("Internet connectivity: FULL");
        ui_draw_info("Online services (Wiimmfi, WiiLink, etc.) should work");
      } else if (dns_ok || http_ok) {
        ui_draw_warn("Internet connectivity: PARTIAL");
        ui_draw_info("Some services may not work correctly");
      } else {
        ui_draw_err("Internet connectivity: NONE");
        ui_draw_warn("Connected to WiFi but cannot reach internet");
        ui_draw_info("Check router settings / firewall");
      }
    } else {
      ui_printf("   " UI_WHITE
                "Skipping connection tests (no IP address)\n" UI_RESET);
    }

    /* Release network so WD can use the radio */
    net_deinit();
  }

  /* ======================================================================
   * PART 2: WiFi Card Info & AP Scan (after network released)
   * ====================================================================== */

  delay_vsyncs(60); /* Give IOS time to release WiFi */

  ui_draw_section("WiFi Card Information");
  ui_draw_info("Scanning WiFi card and nearby access points...");
  ui_printf("\n");

  {
    char mac_str[32];
    char chan_buf[256];
    bool wd_ready = false;

    /* Initial driver probe: Use mode 0 first for hardware info retrieval */
    if (WD_Init(0) == 0)
      wd_ready = true;
    if (!wd_ready && WD_Init(AOSSAPScan) == 0)
      wd_ready = true;

    if (!wd_ready) {
      ui_draw_err("WiFi driver unavailable (WD_Init failed)");
      rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                       "WiFi Driver Init: FAILED\n");
    } else {
      s_wifi_driver_ok = true;
      delay_vsyncs(30);

      /* --- WiFi Card Info --- */
      memset(&s_wdinfo, 0, sizeof(s_wdinfo));
      if (WD_GetInfo(&s_wdinfo) == 0) {
        int ci, ch_pos = 0;

        bool mac_ok = false;
        int i;
        for (i = 0; i < 6; i++) {
          if (s_wdinfo.MAC[i] != 0 && s_wdinfo.MAC[i] != 0xFF) {
            mac_ok = true;
            break;
          }
        }

        if (mac_ok && s_wdinfo.channel >= 1 && s_wdinfo.channel <= 14) {
          s_wdinfo_valid = true;
          mac_to_str(s_wdinfo.MAC, mac_str);
          ui_draw_kv("MAC Address", mac_str);

          /* Safety: ensure version is a clean string */
          s_wdinfo.version[sizeof(s_wdinfo.version) - 1] = '\0';
          for (ci = 0; ci < (int)sizeof(s_wdinfo.version); ci++) {
            u8 c = s_wdinfo.version[ci];
            if (c == 0)
              break;
            if (c < 0x20 || c > 0x7E)
              s_wdinfo.version[ci] = '?'; /* Mask garbage */
          }
          ui_draw_kv("Firmware", (const char *)s_wdinfo.version);

          {
            /* Only show as country code if both bytes are printable ASCII */
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
                ch_pos += snprintf(chan_buf + ch_pos, sizeof(chan_buf) - ch_pos,
                                   ", ");
              ch_pos += snprintf(chan_buf + ch_pos, sizeof(chan_buf) - ch_pos,
                                 "%d", ci);
            }
          }
          if (ch_pos > 0)
            ui_draw_kv("Enabled Channels", chan_buf);

          ui_draw_ok("WiFi card info retrieved");

          rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                           "MAC Address:         %s\n"
                           "Firmware:            %s\n"
                           "Current Channel:     %d\n"
                           "Enabled Channels:    %s\n",
                           mac_str, (const char *)s_wdinfo.version,
                           s_wdinfo.channel, chan_buf);
        } else {
          ui_draw_warn("WiFi card info invalid or uninitialized");
          rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                           "WiFi Card Info:      READ FAILED (Invalid data)\n");
          /* s_wifi_driver_ok stays true; we just mark the data as failed */
        }
      } else {
        ui_draw_err("Failed to read WiFi card info");
        rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                         "WiFi Card Info:      FAILED\n");
      }

      /* --- AP Scan: Re-init with AOSSAPScan if needed --- */
      ui_draw_section("WiFi AP Scan");
      ui_draw_info("Scanning for nearby access points...");

      /* Some IOS versions need AOSSAPScan mode specifically for ScanOnce */
      WD_Deinit();
      delay_vsyncs(30);
      WD_Init(AOSSAPScan);

      {
        ScanParameters sparams;
        s32 scan_ret;

        WD_SetDefaultScanParameters(&sparams);
        sparams.MaxChannelTime = 400;
        sparams.ChannelBitmap = 0x3FFF;
        memset(s_scan_buf, 0, sizeof(s_scan_buf));

        scan_ret = WD_ScanOnce(&sparams, s_scan_buf, sizeof(s_scan_buf));

        /* Retry once if first scan returned empty */
        if (scan_ret >= 0 && s_scan_buf[0] == 0 && s_scan_buf[1] == 0) {
          delay_vsyncs(45);
          scan_ret = WD_ScanOnce(&sparams, s_scan_buf, sizeof(s_scan_buf));
        }

        do_ap_scan(&rpos, s_scan_buf, scan_ret);
      }
      /* IMPORTANT: Release WD driver so net_init can use the hardware again */
      WD_Deinit();
      delay_vsyncs(60);
    }
  }

  /* Retry connectivity after WD released the driver (fixes -24 and -116) */
  if (!s_wifi_working) {
    ui_draw_section("Network Connectivity (Final Attempt)");
    ui_draw_info("Retrying after driver release...");
    delay_vsyncs(60);
    net_deinit();
    delay_vsyncs(30);
    s_net_done = false;
    LWP_CreateThread(&s_net_thread, net_init_thread_func, NULL, s_net_stack,
                     sizeof(s_net_stack), 64);
    while (!s_net_done) {
      VIDEO_WaitVSync();
    }
    ret = s_net_ret;
    if (ret >= 0) {
      s_wifi_working = true;
      ui_draw_ok("Network connected on retry");

      ui_draw_section("IP Configuration");
      {
        u32 ip = net_gethostip();
        if (ip != 0) {
          u8 first_octet, second_octet;
          s_ip_obtained = true;
          ip_to_str(ip, s_ip_str);
          ui_draw_kv("IP Address", s_ip_str);
          ui_draw_kv("Config Method", "Obtained via DHCP");
          first_octet = (ip >> 24) & 0xFF;
          second_octet = (ip >> 16) & 0xFF;
          if (first_octet == 192 && second_octet == 168)
            ui_draw_ok("Valid private IP range (192.168.x.x)");
          else if (first_octet == 10)
            ui_draw_ok("Valid private IP range (10.x.x.x)");
          else if (first_octet == 172 && second_octet >= 16 &&
                   second_octet <= 31)
            ui_draw_ok("Valid private IP range (172.16-31.x.x)");
          else if (first_octet == 169 && second_octet == 254)
            ui_draw_warn("Link-local IP (169.254.x.x) - DHCP may have failed");
        } else {
          ui_draw_err("No IP address obtained");
          ui_draw_warn("WiFi connected but DHCP failed");
        }
      }

      ui_draw_section("Connection Tests");
      if (s_ip_obtained) {
        bool dns_ok =
            test_tcp_connection("Google DNS (8.8.8.8:53)", 0x08080808, 53);
        bool http_ok =
            test_tcp_connection("HTTP Test (1.1.1.1:80)", 0x01010101, 80);
        ui_printf("\n");
        if (dns_ok && http_ok) {
          ui_draw_ok("Internet connectivity: FULL");
          ui_draw_info("Online services (Wiimmfi, WiiLink, etc.) should work");
        } else if (dns_ok || http_ok) {
          ui_draw_warn("Internet connectivity: PARTIAL");
          ui_draw_info("Some services may not work correctly");
        } else {
          ui_draw_err("Internet connectivity: NONE");
          ui_draw_warn("Connected to WiFi but cannot reach internet");
          ui_draw_info("Check router settings / firewall");
        }
      } else {
        ui_printf("   " UI_WHITE
                  "Skipping connection tests (no IP address)\n" UI_RESET);
      }

      net_deinit();
    } else {
      connectivity_ret = ret;
      {
        char retry_msg[80];
        snprintf(retry_msg, sizeof(retry_msg), "Retry failed (error %d)", ret);
        ui_draw_warn(retry_msg);
      }
      if (ret == -24) {
        ui_draw_info(
            "Set up WiFi in Wii Settings -> Internet -> Connection Settings");
        ui_draw_info("and run the connection test there.");
      } else if (ret == -116) {
        ui_draw_info("Error -116: timeout or no response from router.");
      }
      net_deinit();
    }
  }

  if (s_wifi_working) {
    rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                     "\n=== NETWORK CONNECTIVITY ===\nWiFi Status: OK\n");
  } else {
    if (connectivity_ret == -24)
      rpos += snprintf(
          s_report + rpos, sizeof(s_report) - rpos,
          "\n=== NETWORK CONNECTIVITY ===\nWiFi Status: Not connected (error "
          "-24)\n"
          "  (normal when no connection is configured in Wii Settings)\n");
    else
      rpos += snprintf(
          s_report + rpos, sizeof(s_report) - rpos,
          "\n=== NETWORK CONNECTIVITY ===\nWiFi Status: FAILED (error %d)\n",
          connectivity_ret);
    if (connectivity_ret == -116)
      rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                       "  (error -116 = timeout / no response from router; AP "
                       "scan still succeeded)\n");
  }

  /* Tips */
  ui_draw_section("WiFi Notes");
  ui_draw_info("Wii only supports 802.11b/g (2.4GHz)");
  ui_draw_info("WPA2-PSK (AES) is recommended for security");
  ui_draw_info("WPA3 and 5GHz networks are NOT supported");
  ui_draw_info("For Wiimmfi, ports 28910 and 29900-29901 must be open");

  /* Finalize header (patch placeholders with final status) */
  {
    char *p = strstr(s_report, "Searching...   ");
    if (p) {
      char tmp[16];
      snprintf(tmp, sizeof(tmp), "%-14s",
               s_wifi_driver_ok ? "Working" : "Failed");
      memcpy(p, tmp, 14); /* Overwrite without null terminator */

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

/*---------------------------------------------------------------------------*/
void get_network_test_report(char *buf, int bufsize) {
  strncpy(buf, s_report, bufsize - 1);
  buf[bufsize - 1] = '\0';
}
