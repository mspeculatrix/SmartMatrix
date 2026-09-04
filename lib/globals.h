/**
 * @file globals.h
 * @brief Global variables and state
 */

#ifndef __SMARTMATRIX_GLOBALS_H__
#define __SMARTMATRIX_GLOBALS_H__

#include "defines.h"
#include "pico_ssd1306_basic.h"
#include "wifi_creds.h"

 // =============================================================================
 // GLOBAL STATE
 // =============================================================================

// ----- FOR NEXT VERSION ------------------------------------------------------
// SystemContext* sys_ctx = new SystemContext{};
// NetworkContext* net_ctx = new NetworkContext{};

// int app_init(void) {
// 	if (sys_ctx && net_ctx) {
// 		// Initialize system context fields
// 		sys_ctx->cmd_idx = 0;
// 		sys_ctx->system_status = STATUS_IDLE;
// 		sys_ctx->serial_mode = STATE_COMMAND;
// 		sys_ctx->current_err_state = ERR_NONE;
// 		sys_ctx->system_status_msg[0] = "IDLE/READY";
// 		sys_ctx->system_status_msg[1] = "PRINTING";
// 		sys_ctx->system_status_msg[2] = "ERROR";
// 		sys_ctx->autofeed_cfg = AF_OFF;

// 		// Initialize network context fields
// 		net_ctx->netif = &cyw43_state.netif[CYW43_ITF_STA];
// 		net_ctx->wifi_connected = false;
// 		snprintf(net_ctx->ip_buf, sizeof(net_ctx->ip_buf), "--");
// 		strncpy(net_ctx->wifi_ssid, WIFI_SSID, sizeof(net_ctx->wifi_ssid) - 1);
// 		// Ensure null-termination
// 		net_ctx->wifi_ssid[sizeof(net_ctx->wifi_ssid) - 1] = '\0';
// 		strncpy(net_ctx->wifi_passwd, WIFI_PASSWORD, sizeof(net_ctx->wifi_passwd) - 1);
// 		// Ensure null-termination
// 		net_ctx->wifi_passwd[sizeof(net_ctx->wifi_passwd) - 1] = '\0';
// 	} else {
// 		printf("! ERR: App init failed\n");
// 		return -1;
// 	}
// 	return 0;
// }



/// @brief System state
SystemStatus system_status = STATUS_IDLE;
const char* system_status_msg[] = { "IDLE/READY", "PRINTING", "ERROR" };
ErrorState current_err_state = ERR_NONE;

/// @brief Serial mode - CLI/command or printing
SerialMode serial_mode = STATE_COMMAND; // Default to CLI mode



/// @brief Wifi/network
// Max SSID length is 32 bytes plus terminator (per 802.11 spec)
char wifi_ssid[WIFI_SSID_LEN + 1] = WIFI_SSID;
// Max password length is 63 ASCII or 64 hex chars plus terminator
char wifi_passwd[WIFI_PASS_LEN + 1] = WIFI_PASSWORD;
struct netif* netif = &cyw43_state.netif[CYW43_ITF_STA]; // Network interface
bool wifi_connected = false;

/// @brief Error messages for OLED. Also see ErrorState enum in defines.h
const char* error_msg[] = { "OK", "OFFLINE", "ERROR", "PAPER OUT", "NO WIFI" };

/// @brief In-band command accumulation buffer
char cmd_buffer[CMD_BUF_LEN];

/// @brief String to hold IP address
char ip_buf[IP_BUF_SIZE];

/// @brief Current index within the command buffer
size_t cmd_idx = 0;

/// @brief Autofeed configuration
AutoFeed autofeed_cfg = AF_OFF;                // Active low; Disabled by default

/** @brief FIFO message vars
 *
 * Marked as volatile since these are shared across core contexts
*/
volatile uint32_t total_job_bytes = 0;
volatile bool is_printing = false;

/// @brief OLED display
ssd1306_t display;

#endif
