#ifndef __SMARTMATRIX_DEFINES_H__
#define __SMARTMATRIX_DEFINES_H__

// ============================================================================
// CONFIGURATION CONSTANTS
// ============================================================================

#define VERSION_STR "Version 0.9.0"

/** @brief Standard JetDirect / RAW TCP printing port */
#define TCP_PORT 9100

/** @brief Hostname broadcast over DHCP/mDNS */
#define NET_IF_HOSTNAME "smartmatrix"

#define IP_BUF_SIZE 20

/** @brief Maximum retry attempts for initial network association */
#define WIFI_MAX_TRIES 5

// --- IEEE 1284 handshake timings ---
#define STROBE_DURATION 1   // µs - Minimum active-low STROBE pulse width
#define HOLD_DURATION   1   // µs - Setup/hold time before/after STROBE edge
#define RESET_DURATION  100 // ms - Active-low INIT pulse for hardware reset

// --- In-band command delimiters ---
// #define CMD_MODE_START 0x01 // Start of Header: Switch stream to command mode
#define PRT_MODE_END   0x04 // End of Transmission: Return stream to print mode

#define CHAR_LF 0x0A		// Linefeed
#define CHAR_CR 0x0D		// Carriage return

/**
 * @brief Bitmask restricting data writes strictly to GPIO 0 through 7.
 * @note CRITICAL: Must stay strictly 0xFF so gpio_put_masked does not touch
 * higher GPIOs reserved for internal CYW43 SPI communications (GPIO 23-25, 29).
 */
#define DATA_MASK       0xFF

 // ============================================================================
 // PIN ASSIGNMENTS
 // ============================================================================

  // --- IEEE 1284 parallel outputs ---
#define STROBE_PIN      8   // Active Low: Signals valid data on GPIO 0-7
#define INIT_PIN        9   // Active Low: Hardware printer reset pulse
#define AUTOFEED_PIN    10  // Active Low: Directs printer to line-feed on CR

// --- IEEE 1284 parallel inputs ---
#define BUSY_PIN        11  // High = Printer buffer full or processing
#define ACK_PIN         12  // Active Low: Pulse indicating character accepted
#define PAPER_END_PIN   13  // High = Out of paper
#define SELECT_PIN      14  // High = Printer online and selected
#define ERROR_PIN       15  // Active Low: General fault (paper jam, cover open)

// --- I2C & indicator output pins ---
#define I2C_PORT        	i2c0
#define I2C_SDA_PIN     	20  // Note: can't use GPIO 16 = CYW43 Power Enable
#define I2C_SCL_PIN     	21  // Note: can't use GPIO 17 (CYW43 conflict)
#define ACTIVITY_LED_PIN   	18  // Core activity indicator
#define WIFI_LED_PIN       	19  // Character transmission toggle indicator

// --- OLED display ---
// #define DISPLAY_WIDTH 128
// #define DISPLAY_HEIGHT 64
#define DISPLAY_I2C_ADDR 0x3C // Standard I2C address for 0.96" SSD1306

// --- FIFO PROTOCOL DISCRIMINATORS
// These help disambiguate Core 0 vs Core 1 messages
#define FIFO_TAG_DATA		0x00000000 // Core 0 -> 1: Print byte (Bit 31=0)
#define FIFO_TAG_MSG 		0x80000000 // Core 1 -> 0: Message (Bit 31 = 1)

// --- FIFO MESSAGE PROTOCOL (Core 1 -> Core 0) ---
#define FIFO_MSG_MASK_TYPE	0x7F000000 // Mask to extract type (msg or data)
#define FIFO_MSG_MASK_DATA	0x00FFFFFF // Mask to extract data

#define FIFO_MSG_JOB_START	0x01000000
#define FIFO_MSG_JOB_END	0x02000000
#define FIFO_MSG_BYTE_COUNT	0x03000000

/**
 * @brief State machine enum for parsing in-band printer control commands.
 */
enum SerialMode {
	STATE_PRINTING, /**< Pass-through mode: bytes routed to printer */
	STATE_COMMAND   /**< CLI mode: bytes captured into buffer */
};

enum CommandState {
	// In handle_command() this determines how the latest incoming string is
	// treated:
	CMD_COMMAND,			// As a command */
	CMD_SSID,				// As an SSID name
	CMD_PASSWD				// As a Wifi password
};

enum SystemStatus {
	STATUS_IDLE,
	STATUS_PRINTING,
	STATUS_ERROR
};

enum ErrorState {
	ERR_NONE,
	ERR_OFFLINE,
	ERR_GEN,
	ERR_PE,
	ERR_WIFI
};

enum AutoFeed { AF_ON, AF_OFF };

#endif
