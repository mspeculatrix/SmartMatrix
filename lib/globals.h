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

 /// @brief System state
SystemStatus system_status = STATUS_IDLE;
const char* system_status_msg[] = { "IDLE/READY", "PRINTING", "ERROR" };
ErrorState current_err_state = ERR_NONE;

/// @brief Serial mode - CLI/command or printing
static SerialMode serial_mode = STATE_COMMAND; // Default to CLI mode

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
char cmd_buffer[256];

/// @brief String to hold IP address
char ip_buf[IP_BUF_SIZE];

/// @brief Autofeed configuration
AutoFeed autofeed_cfg = AF_OFF;                // Active low; Disabled by default

/** @brief FIFO message vars
 *
 * Marked as volatile since these are shared across core contexts
*/
volatile uint32_t total_job_bytes = 0;
volatile bool is_printing = false;

/// @brief Current index within the command buffer
size_t cmd_idx = 0;

/// @brief OLED display
ssd1306_t display;

#endif
