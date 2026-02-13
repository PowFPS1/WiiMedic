/*
 * WiiMedic - network_test.c
 * Tests WiFi module, connection status, IP configuration
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <gccore.h>
#include <network.h>
#include <ogc/lwp_watchdog.h>

#include "network_test.h"
#include "ui_common.h"

static char s_report[2048];
static bool s_wifi_working = false;
static bool s_ip_obtained  = false;
static char s_ip_str[32]   = "N/A";

/*---------------------------------------------------------------------------*/
static void ip_to_str(u32 ip, char *buf) {
    sprintf(buf, "%d.%d.%d.%d",
            (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
            (ip >> 8)  & 0xFF, ip & 0xFF);
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
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = htonl(host_ip);

    start = gettime();
    ret   = net_connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    end   = gettime();

    latency_ms = (float)ticks_to_millisecs(end - start);
    net_close(sock);

    if (ret >= 0) {
        snprintf(buf, sizeof(buf), "%s: Connected (%.0f ms)", host_desc, latency_ms);
        ui_draw_ok(buf);
        return true;
    } else {
        snprintf(buf, sizeof(buf), "%s: Connection failed (error %d)", host_desc, ret);
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
    s_ip_obtained  = false;

    ui_draw_info("Initializing network interface...");
    ui_draw_info("This may take up to 15 seconds...");
    ui_printf("\n");

    ret = net_init();

    if (ret < 0) {
        {
            char msg[128];
            snprintf(msg, sizeof(msg), "Network initialization failed (error %d)", ret);
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
            "=== NETWORK TEST ===\nWiFi Status: FAILED (error %d)\n\n", ret);
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

            first_octet  = (ip >> 24) & 0xFF;
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
        bool dns_ok  = test_tcp_connection("Google DNS (8.8.8.8:53)",  0x08080808, 53);
        bool http_ok = test_tcp_connection("HTTP Test (1.1.1.1:80)",   0x01010101, 80);

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
        ui_printf("   " UI_WHITE "Skipping connection tests (no IP address)\n" UI_RESET);
    }

    /* Tips */
    ui_draw_section("WiFi Notes");
    ui_draw_info("Wii only supports 802.11b/g (2.4GHz)");
    ui_draw_info("WPA2-PSK (AES) is recommended for security");
    ui_draw_info("WPA3 and 5GHz networks are NOT supported");
    ui_draw_info("For Wiimmfi, ports 28910 and 29900-29901 must be open");

    /* Report */
    rpos += snprintf(s_report + rpos, sizeof(s_report) - rpos,
        "=== NETWORK TEST ===\n"
        "WiFi Module:         %s\n"
        "IP Address:          %s\n"
        "IP Obtained:         %s\n"
        "\n",
        s_wifi_working ? "Working" : "Failed",
        s_ip_str,
        s_ip_obtained ? "Yes" : "No"
    );

    net_deinit();

    ui_printf("\n");
    ui_draw_ok("Network test complete");
}

/*---------------------------------------------------------------------------*/
void get_network_test_report(char *buf, int bufsize) {
    strncpy(buf, s_report, bufsize - 1);
    buf[bufsize - 1] = '\0';
}
