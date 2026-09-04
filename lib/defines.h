/**
 * @file defines.h
 * @brief Constants, enums and inline functions
 */

#ifndef __SMARTMATRIX_DEFINES_H__
#define __SMARTMATRIX_DEFINES_H__

#include "pico/multicore.h"

#define VERSION_STR "Version 0.9.2"

 /// @brief Standard JetDirect / RAW TCP printing port
#define TCP_PORT 9100

/// @brief Hostname broadcast over DHCP/mDNS
#define NET_IF_HOSTNAME "smartmatrix"

#define IP_BUF_SIZE 20

/// @brief Maximum retry attempts for initial network association
#define WIFI_MAX_TRIES 5
/// @brief Maximum length of SSID name (not incl terminator) per 802.11 spec
#define WIFI_SSID_LEN 32
/// @brief Maximum length of wifi password (not including terminator)
#define WIFI_PASS_LEN 64

/// @brief IEEE 1284 handshake timings
#define STROBE_DURATION 1   // µs - Minimum active-low STROBE pulse width
#define HOLD_DURATION   1   // µs - Setup/hold time before/after STROBE edge
#define RESET_DURATION  100 // ms - Active-low INIT pulse for hardware reset

/// @brief In-band command delimiters
#define PRT_MODE_END   0x04 // End of Transmission: Return stream to print mode

#define CHAR_LF 0x0A		// Linefeed
#define CHAR_CR 0x0D		// Carriage return

/**
 * @brief Bitmask restricting data writes strictly to GPIO 0 through 7.
 * @note CRITICAL: Must stay strictly 0xFF so gpio_put_masked does not touch
 * higher GPIOs reserved for internal CYW43 SPI communications (GPIO 23-25, 29).
 */
#define DATA_MASK       0xFF

 /// @section PIN ASSIGNMENTS

 /// @brief IEEE 1284 parallel port outputs
#define STROBE_PIN      8   // Active Low: Signals valid data on GPIO 0-7
#define INIT_PIN        9   // Active Low: Hardware printer reset pulse
#define AUTOFEED_PIN    10  // Active Low: Directs printer to line-feed on CR

/// @brief IEEE 1284 parallel port inputs
#define BUSY_PIN        11  // High = Printer buffer full or processing
#define ACK_PIN         12  // Active Low: Pulse indicating character accepted
#define PAPER_END_PIN   13  // High = Out of paper
#define SELECT_PIN      14  // High = Printer online and selected
#define ERROR_PIN       15  // Active Low: General fault (paper jam, cover open)

/// @brief I2C & indicator output pins
#define I2C_PORT        	i2c0
#define I2C_SDA_PIN     	20  // Note: can't use GPIO 16 = CYW43 Power Enable
#define I2C_SCL_PIN     	21  // Note: can't use GPIO 17 (CYW43 conflict)
#define ACTIVITY_LED_PIN   	18  // Core activity indicator
#define WIFI_LED_PIN       	19  // Character transmission toggle indicator

/// @section MISC settings

/// @brief OLED display
// #define DISPLAY_WIDTH 128
// #define DISPLAY_HEIGHT 64
#define DISPLAY_I2C_ADDR 0x3C // Standard I2C address for 0.96" SSD1306

/**
 * @brief FIFO protocol discriminators/masks
 *
 * These help disambiguate Core 0 vs Core 1 messages
 */
#define FIFO_TAG_DATA		0x00000000 // Core 0 -> 1: Print byte (Bit 31=0)
#define FIFO_TAG_MSG 		0x80000000 // Core 1 -> 0: Message (Bit 31 = 1)

 // --- FIFO MESSAGE PROTOCOL (Core 1 -> Core 0) ---
#define FIFO_MSG_MASK_TYPE	0x7F000000 // Mask to extract type (msg or data)
#define FIFO_MSG_MASK_DATA	0x00FFFFFF // Mask to extract data

#define FIFO_MSG_JOB_START	0x01000000
#define FIFO_MSG_JOB_END	0x02000000
#define FIFO_MSG_BYTE_COUNT	0x03000000

#define CMD_BUF_LEN 256

/// @section ENUMs

/// @brief State machine enum for parsing in-band printer control commands.
enum SerialMode {
	STATE_PRINTING, 			// Pass-through mode: bytes routed to printer
	STATE_COMMAND 				// CLI mode: bytes captured into buffer
};

/// @brief Current mode of CLI interpreter
enum CommandState {
	// In handle_command() this determines how the latest incoming string is
	// treated:
	CMD_COMMAND,				// As a command
	CMD_SSID,					// As an SSID name
	CMD_PASSWD					// As a Wifi password
};

/// @brief Printer state
enum SystemStatus {
	STATUS_IDLE,				// These correspond to the status messages
	STATUS_PRINTING,			// in system_status_msg[] array, defined in
	STATUS_ERROR				// globals.h
};

/// @brief Current printer error state
enum ErrorState {
	ERR_NONE,					// These correspond to the error messages
	ERR_OFFLINE,				// in global error_msg[] array, defined in
	ERR_GEN,					// globals.h
	ERR_PE,
	ERR_WIFI
};

/// @brief Setting of Autofeed signal
enum AutoFeed { AF_ON, AF_OFF };

/// @brief Context structs ----- FOR NEXT VERSION -----------------------------
// typedef struct {
// 	SystemStatus system_status;
// 	SerialMode serial_mode;
// 	ErrorState current_err_state;
// 	const char* system_status_msg[3];
// 	char cmd_buffer[CMD_BUF_LEN];
// 	size_t cmd_idx;
// 	AutoFeed autofeed_cfg;
// } SystemContext;

// typedef struct {
// 	char wifi_passwd[WIFI_PASS_LEN + 1];
// 	char wifi_ssid[WIFI_SSID_LEN + 1];
// 	char ip_buf[IP_BUF_SIZE];
// 	bool wifi_connected;
// 	struct netif* netif;
// } NetworkContext;

/// @section FIFO inline functions

/// @brief Sends messages over FIFO from Core 1 to Core 0
inline void send_fifo_msg(uint32_t type, uint32_t payload) {
	uint32_t msg = FIFO_TAG_MSG | type | (payload & FIFO_MSG_MASK_DATA);
	multicore_fifo_push_blocking(msg);
}

/// @brief Sends bytes for printing from Core 0 to Core 1
inline void send_print_byte_to_core1(uint8_t byte) {
	uint32_t msg = FIFO_TAG_DATA | (uint32_t)byte;
	multicore_fifo_push_blocking(msg);
}

#endif
