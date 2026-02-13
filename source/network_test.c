/*
 * WiiMedic - network_test.c
 * Tests WiFi module, connection status, IP configuration,
 * WiFi card info, and nearby AP scanning (libogc 3.0.0+)
 */

#include <errno.h>
#include <gccore.h>
#include <network.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/wd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "network_test.h"
#include "ui_common.h"

/* Max APs to display from scan results */
#define MAX_SCAN_APS 16
/* Buffer for raw scan data (BSSDescriptors + IEs) */
#define SCAN_BUF_SIZE 4096

static char s_report[4096];
static bool s_wifi_working = false;
static bool s_ip_obtained = false;
static char s_ip_str[32] = "N/A";

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
  /* Check if the network is secured via capability flags */
  if (!(bss->Capabilities & CAPAB_SECURED_FLAG))
    return "Open";

  /* Try to detect WPA2/RSN by looking for IE 0x30 (RSN) */
  if (WD_GetIELength(bss, IEID_SECURITY) > 0)
    return "WPA2";

  return "WEP/WPA";
}

/*---------------------------------------------------------------------------*/
static const char *get_signal_str(u8 level) {
  /* WD_GetRadioLevel returns a value; interpret it */
  if (level == 0)
    return "Weak";
  if (level == 1)
    return "Fair";
  if (level == 2)
    return "Good";
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
void run_network_test(void) {
  int rpos = 0;
  s32 ret;

  memset(s_report, 0, sizeof(s_report));
  s_wifi_working = false;
  s_ip_obtained = false;

  ui_draw_info("Initializing network interface...");
  ui_draw_info("This may take up to 15 seconds...");
  ui_printf("\n");

  ret = net_init();

  if (ret < 0) {
    {
      char msg[128];
      snprintf(msg, sizeof(msg), "Network initialization failed (error %d)",
               ret);
      ui_draw_err(msg);
    }
    ui_printf("\n");

    switch (ret) {
    case -EAGAIN:
      ui_draw_warn("Network module busy - try again");
      break;
    case -6:
      ui_draw_warn("No wireless network configured");
      ui_draw_info("Configure WiFi in Wii System Settings first");
      break;
    default:
      ui_draw_warn("WiFi module may be damaged or not configured");
      break;
    }

    rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                     "=== NETWORK TEST ===\nWiFi Status: FAILED (error %d)\n\n",
                     ret);
    return;
  }

  s_wifi_working = true;
  ui_draw_ok("WiFi module initialized successfully");

  /* IP configuration */
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

  /* Connection tests */
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

  /* ---------------------------------------------------------------
   * WiFi Card Information (via WD driver, libogc 3.0.0+)
   * --------------------------------------------------------------- */
  ui_draw_section("WiFi Card Information");

  /* We need to deinit the network stack first, then lock the wireless
   * driver for direct WD access. After WD work, we release it. */
  net_deinit();

  {
    s32 lockid;
    WDInfo wdinfo;
    char mac_str[20];
    char chan_buf[128];
    bool wd_ok = false;
    int scan_count = 0;

    lockid = NCD_LockWirelessDriver();
    if (lockid < 0) {
      char msg[80];
      snprintf(msg, sizeof(msg), "Could not lock WiFi driver (error %d)",
               lockid);
      ui_draw_err(msg);
      rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                       "WiFi Driver Lock: FAILED (error %d)\n", lockid);
    } else {
      /* Initialize WD in AOSSAPScan mode (mode 3) for scanning */
      if (WD_Init(AOSSAPScan) == 0) {
        wd_ok = true;

        /* --- WiFi Card Info --- */
        memset(&wdinfo, 0, sizeof(wdinfo));
        if (WD_GetInfo(&wdinfo) == 0) {
          int ci, ch_pos = 0;

          mac_to_str(wdinfo.MAC, mac_str);
          ui_draw_kv("MAC Address", mac_str);

          /* Firmware version (may contain trailing junk) */
          wdinfo.version[sizeof(wdinfo.version) - 1] = '\0';
          ui_draw_kv("Firmware", (const char *)wdinfo.version);

          /* Country code */
          {
            char cc[8];
            snprintf(cc, sizeof(cc), "%c%c",
                     wdinfo.CountryCode[0] ? wdinfo.CountryCode[0] : '?',
                     wdinfo.CountryCode[1] ? wdinfo.CountryCode[1] : '?');
            ui_draw_kv("Country Code", cc);
          }

          /* Current channel */
          {
            char ch_str[8];
            snprintf(ch_str, sizeof(ch_str), "%d", wdinfo.channel);
            ui_draw_kv("Current Channel", ch_str);
          }

          /* Supported channels from bitmask */
          chan_buf[0] = '\0';
          for (ci = 1; ci <= 14; ci++) {
            if (wdinfo.EnableChannelsMask & (1 << (ci - 1))) {
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
                           mac_str, (const char *)wdinfo.version,
                           wdinfo.channel, chan_buf);
        } else {
          ui_draw_err("Failed to read WiFi card info");
          rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                           "WiFi Card Info:      FAILED\n");
        }

        /* --- WiFi AP Scan --- */
        ui_draw_section("WiFi AP Scan");
        ui_draw_info("Scanning for nearby access points...");

        {
          static u8 scan_buf[SCAN_BUF_SIZE] ATTRIBUTE_ALIGN(32);
          ScanParameters sparams;
          s32 scan_ret;

          WD_SetDefaultScanParameters(&sparams);
          memset(scan_buf, 0, sizeof(scan_buf));

          scan_ret = WD_ScanOnce(&sparams, scan_buf, sizeof(scan_buf));

          if (scan_ret >= 0) {
            /* Parse BSSDescriptor entries from the buffer */
            u8 *ptr = scan_buf;
            u8 *end = scan_buf + sizeof(scan_buf);

            scan_count = 0;

            rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                             "\n--- Nearby Access Points ---\n");

            while (ptr < end && scan_count < MAX_SCAN_APS) {
              BSSDescriptor *bss = (BSSDescriptor *)ptr;
              char ssid[33];
              char bssid_str[20];
              char line[128];
              u8 signal;
              u16 entry_len;

              /* Validate entry */
              if (bss->length == 0 || bss->length < sizeof(BSSDescriptor))
                break;

              entry_len = bss->length;

              /* Extract SSID */
              memset(ssid, 0, sizeof(ssid));
              if (bss->SSIDLength > 0 && bss->SSIDLength <= 32)
                memcpy(ssid, bss->SSID, bss->SSIDLength);
              else
                strcpy(ssid, "(Hidden)");

              mac_to_str(bss->BSSID, bssid_str);
              signal = WD_GetRadioLevel(bss);

              snprintf(line, sizeof(line), "%-24s Ch:%-2d  Sig:%s  %s", ssid,
                       bss->channel, get_signal_str(signal),
                       get_security_str(bss));

              /* Color based on signal */
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
              ptr += entry_len;
            }

            if (scan_count == 0) {
              ui_draw_warn("No access points found");
              rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                               "  (none found)\n");
            } else {
              char cnt[64];
              snprintf(cnt, sizeof(cnt), "Found %d access point(s)",
                       scan_count);
              ui_draw_ok(cnt);
            }
          } else {
            char msg[80];
            snprintf(msg, sizeof(msg), "AP scan failed (error %d)", scan_ret);
            ui_draw_err(msg);
            rpos +=
                snprintf(s_report + rpos, sizeof(s_report) - rpos,
                         "AP Scan:             FAILED (error %d)\n", scan_ret);
          }
        }

        WD_Deinit();
      } else {
        ui_draw_err("Failed to initialize WiFi driver (WD_Init)");
        rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
                         "WiFi Driver Init:    FAILED\n");
      }

      NCD_UnlockWirelessDriver(lockid);
    }
  }

  /* Tips */
  ui_draw_section("WiFi Notes");
  ui_draw_info("Wii only supports 802.11b/g (2.4GHz)");
  ui_draw_info("WPA2-PSK (AES) is recommended for security");
  ui_draw_info("WPA3 and 5GHz networks are NOT supported");
  ui_draw_info("For Wiimmfi, ports 28910 and 29900-29901 must be open");

  /* Report header (prepend to what was already added) */
  {
    char hdr[256];
    int hlen = snprintf(hdr, sizeof(hdr),
                        "=== NETWORK TEST ===\n"
                        "WiFi Module:         %s\n"
                        "IP Address:          %s\n"
                        "IP Obtained:         %s\n",
                        s_wifi_working ? "Working" : "Failed", s_ip_str,
                        s_ip_obtained ? "Yes" : "No");

    /* Shift existing report content and prepend header */
    if (hlen + rpos < (int)sizeof(s_report)) {
      memmove(s_report + hlen, s_report, rpos + 1);
      memcpy(s_report, hdr, hlen);
      rpos += hlen;
    }
  }

  rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos, "\n");

  ui_printf("\n");
  ui_draw_ok("Network test complete");
}

/*---------------------------------------------------------------------------*/
void get_network_test_report(char *buf, int bufsize) {
  strncpy(buf, s_report, bufsize - 1);
  buf[bufsize - 1] = '\0';
}
