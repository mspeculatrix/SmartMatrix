/**
 * @file SmartMatrix.cpp
 * @brief Network-enabled parallel printer interface server for Pico 2W.
 *
 * This application creates a TCP JetDirect server (Port 9100) on Core 0 that
 * receives raw print jobs over wifi via a socket connection and also raw bytes
 * over serial (via USB) and transfers data bytes securely across
 * inter-core FIFOs to Core 1. Core 1 handles real-time bit-banging and
 * handshake timings for standard IEEE 1284 Centronics parallel printers.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/cyw43_arch.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "lwip/tcp.h"
#include "pico_ssd1306_basic.h"
#include "__wifi_creds.h" // Wifi credentials (WIFI_SSID, WIFI_PASSWORD)

 // ============================================================================
 // CONFIGURATION CONSTANTS
 // ============================================================================

#define VERSION_STR "Version 0.9.0"

/** @brief Standard JetDirect / RAW TCP printing port */
#define TCP_PORT 9100

/** @brief Hostname broadcast over DHCP/mDNS */
#define NET_IF_HOSTNAME "smartmatrix"

/** @brief Maximum retry attempts for initial network association */
#define WIFI_MAX_TRIES 5

// --- IEEE 1284 handshake timings ---
#define STROBE_DURATION 1   // µs - Minimum active-low STROBE pulse width
#define HOLD_DURATION   1   // µs - Setup/hold time before/after STROBE edge
#define RESET_DURATION  100 // ms - Active-low INIT pulse for hardware reset

// --- In-band command delimiters ---
#define CMD_MODE_START 0x01 // Start of Header: Switch stream to command mode
#define CMD_MODE_END   0x04 // End of Transmission: Return stream to print mode

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

// =============================================================================
// GLOBAL STATE & ENUMS
// =============================================================================

/**
 * @brief State machine enum for parsing in-band printer control commands.
 */
enum ParserState {
	STATE_PRINTING, /**< Normal pass-through mode: bytes routed to printer */
	STATE_COMMAND   /**< Command intercept mode: bytes captured into buffer */
};

enum SystemStatus {
	STATUS_IDLE,
	STATUS_PRINTING,
	STATUS_ERROR
};

uint8_t system_status = 0;
char* system_status_msg[] = { "IDLE/READY", "PRINTING", "ERROR" };
static uint8_t current_err_state = ERR_NONE;

enum ErrorState {
	ERR_NONE,
	ERR_OFFLINE,
	ERR_GEN,
	ERR_PE
};

enum AutoFeed {
	AF_ON,
	AF_OFF
};

/** @brief In-band command accumulation buffer */
char cmd_buffer[256];

/** @brief String to hold IP address */
char ip_buf[20];

/** @brief Autofeed configuration */
uint8_t autofeed_cfg = AF_OFF;				// Active low; Disabled by default

// FIFO message State
static uint32_t total_job_bytes = 0;
static bool is_printing = false;

/** @brief Current index within the command buffer */
size_t cmd_idx = 0;

/** @brief OLED display */
ssd1306_t display;

// =============================================================================
// FUNCTION IMPLEMENTATIONS
// =============================================================================

/**
 * @brief Configures GPIO directional modes, pull-up/downs, and I2C peripherals.
 *
 * Initialises GPIO 0-7 for character data, output/input control lines for the
 * printer interface, and binds I2C0 to GPIO 20/21.
 */
void init_hardware() {
	// Data Bus (GPIO 0-7) configured as outputs
	for (int i = 0; i < 8; i++) {
		gpio_init(i);
		gpio_set_dir(i, GPIO_OUT);
	}

	// IEEE 1284 output control lines & status LEDs
	const uint32_t outputs[] = { STROBE_PIN, INIT_PIN, AUTOFEED_PIN,
		ACTIVITY_LED_PIN, WIFI_LED_PIN };
	for (uint32_t pin : outputs) {
		gpio_init(pin);
		gpio_set_dir(pin, GPIO_OUT);
	}

	// Set default idle signal states (Centronics active-low idle voltages)
	gpio_put(STROBE_PIN, 1);        // Idle high
	gpio_put(INIT_PIN, 1);          // Idle high (no reset)
	gpio_put(AUTOFEED_PIN, autofeed_cfg);
	gpio_put(ACTIVITY_LED_PIN, 1);  // On, but probably not long enough to see
	gpio_put(WIFI_LED_PIN, 0);      // Off by default

	// IEEE 1284 input status lines
	const uint32_t inputs[] = { BUSY_PIN, ACK_PIN, PAPER_END_PIN, SELECT_PIN,
		ERROR_PIN };
	for (uint32_t pin : inputs) {
		gpio_init(pin);
		gpio_set_dir(pin, GPIO_IN);
		gpio_disable_pulls(pin); // We have external pull-ups on the PCB
	}

	// I2C Init
	i2c_init(I2C_PORT, 400 * 1000);
	gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
	gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(I2C_SDA_PIN);
	gpio_pull_up(I2C_SCL_PIN);
	gpio_put(ACTIVITY_LED_PIN, 0);
}

/**
 * @brief Initialises the SSD1306 display over I2C0 (GPIO 20/21).
 */
void init_display() {
	// Updated call signature for ssd1306_mini
	ssd1306_init(&display, I2C_PORT);
	ssd1306_clear(&display);
	ssd1306_printstr_double(&display, 0, 0, "BOOTING...");
	ssd1306_println(&display, 3, "SMARTMATRIX");
	ssd1306_println(&display, 5, VERSION_STR);
	ssd1306_show(&display);
}

/**
 * @brief Renders network and job status onto the OLED screen (Core 0)
 */
void display_activity(const char* header, const char* activity, const char* status) {
	ssd1306_clear(&display);
	ssd1306_printstr_double(&display, 0, 0, header);
	ssd1306_println(&display, 3, activity);
	ssd1306_println(&display, 7, status);
	ssd1306_show(&display);
}

/**
 * @brief Prints FIFO message contents onto the OLED screen (Core 0)
 */
void display_status(uint8_t state, uint32_t data) {
	ssd1306_clear(&display);
	bool show_bytes = false;

	if (state == STATUS_PRINTING) {
		ssd1306_printstr_double(&display, 0, 0, "PRINTING");
		show_bytes = true;
	} else if (state == STATUS_IDLE) {
		ssd1306_printstr_double(&display, 0, 0, "IDLE/READY");
		show_bytes = true;
	} else if (state == STATUS_ERROR) {
		switch (data) {
			case ERR_OFFLINE:
				ssd1306_printstr_double(&display, 0, 0, "OFFLINE");
				break;
			case ERR_GEN:
				ssd1306_printstr_double(&display, 0, 0, "ERROR");
				break;
			case ERR_PE:
				ssd1306_printstr_double(&display, 0, 0, "PAPER OUT");
				break;
		}
	}

	if (show_bytes) {
		ssd1306_println(&display, 3, "BYTES:");
		char count_buf[10];
		snprintf(count_buf, sizeof(count_buf), "%lu", (unsigned long)data);
		ssd1306_printstr_double(&display, 48, 24, count_buf);
	}

	char stat_msg[22];

	if (autofeed_cfg == AF_ON) {
		snprintf(stat_msg, sizeof(stat_msg), "%s", "AUTOFEED");
	} else {
		snprintf(stat_msg, sizeof(stat_msg), "%s", "AF off");
	}

	ssd1306_println(&display, 5, stat_msg);

	ssd1306_println(&display, 7, ip_buf);

	ssd1306_show(&display);
}

/**
 * @brief Sends messages over FIFO from Core 1 to Core 0
 */
inline void send_fifo_msg(uint32_t type, uint32_t payload) {
	uint32_t msg = FIFO_TAG_MSG | type | (payload & FIFO_MSG_MASK_DATA);
	multicore_fifo_push_blocking(msg);
}

/**
 * @brief Sends a raw character byte over FIFO from Core 0 to Core 1
 */
inline void send_print_byte_to_core1(uint8_t byte) {
	uint32_t msg = FIFO_TAG_DATA | (uint32_t)byte;
	multicore_fifo_push_blocking(msg);
}

/**
 * @brief Transmits a single byte to the printer using Centronics handshake.
 *
 * Checks the printer BUSY line, places the data byte on the bus, and asserts
 * the STROBE line for the required timing duration.
 *
 * @param character The 8-bit ASCII or binary byte to send.
 */
void send_byte_to_printer(uint8_t character) {
	// Toggle LED to indicate bit-banging activity
	gpio_put(ACTIVITY_LED_PIN, 1);

	// Block until printer is READY: Signals conditions must be:
	// - BUSY   - LOW
	// - SELECT - HIGH
	// - /ERROR - HIGH
	// - PE     - LOW
	while (gpio_get(BUSY_PIN) || !gpio_get(SELECT_PIN)
		|| gpio_get(PAPER_END_PIN) || !gpio_get(ERROR_PIN)) {
		tight_loop_contents(); // Microcontroller spinlock hint
	}

	// Write byte to lower 8 pins (GPIO 0-7) using atomic bit-masking
	gpio_put_masked(DATA_MASK, (uint32_t)character);
	// IEEE 1284 strobe pulse setup time
	sleep_us(HOLD_DURATION);
	gpio_put(STROBE_PIN, 0); // Assert STROBE
	sleep_us(STROBE_DURATION);
	gpio_put(STROBE_PIN, 1); // De-assert STROBE
	sleep_us(HOLD_DURATION);

	gpio_put(ACTIVITY_LED_PIN, 0);
}

/**
 * @brief Processes in-band ASCII control commands received over the stream.
 *
 * @param buffer Pointer to the null-terminated command string.
 * @param length Length of the string in bytes.
 */
void handle_command(const char* buffer, size_t length) {
	if (strncmp(buffer, "STATUS", length) == 0) {
		printf(system_status_msg[system_status]);
		printf("\n");
	} else if (strncmp(buffer, "RESET", length) == 0) {
		// Assert hardware initialise pulse
		gpio_put(ACTIVITY_LED_PIN, 1);
		gpio_put(INIT_PIN, 0);
		sleep_ms(RESET_DURATION);
		gpio_put(INIT_PIN, 1);
		gpio_put(ACTIVITY_LED_PIN, 0);
		printf("MSG:PRINTER_RESET_EXECUTED\n");
	} else if (strncmp(buffer, "AF_ON", length) == 0) {
		autofeed_cfg = AF_ON;
		gpio_put(AUTOFEED_PIN, autofeed_cfg);
	} else if (strncmp(buffer, "AF_OFF", length) == 0) {
		autofeed_cfg = AF_OFF;
		gpio_put(AUTOFEED_PIN, autofeed_cfg);
	} else {
		printf("ERR:UNKNOWN_COMMAND\n");
	}
}

/**
 * @brief Evaluates bytes in state-machine logic to divert management commands.
 *
 * Filtered printable characters (>= 0x07) are passed straight to the printer.
 * Bytes wrapped in CMD_MODE_START (0x01) and CMD_MODE_END (0x04) are diverted
 * into the command buffer.
 *
 * @param byte Incoming stream byte from Network or USB CDC.
 */
void process_byte(uint8_t byte) {
	static ParserState current_state = STATE_PRINTING;

	if (current_state == STATE_PRINTING) {
		if (byte == CMD_MODE_START) {
			current_state = STATE_COMMAND;
			cmd_idx = 0;
		} else if (byte >= 0x07) {
			if (!is_printing) {
				is_printing = true;
				send_fifo_msg(FIFO_MSG_JOB_START, 0);
			}

			send_byte_to_printer(byte);
			total_job_bytes++;

			if (total_job_bytes % 100 == 0) {
				send_fifo_msg(FIFO_MSG_BYTE_COUNT, total_job_bytes);
			}
		}
	} else if (current_state == STATE_COMMAND) {
		if (byte == CMD_MODE_END) {
			cmd_buffer[cmd_idx] = '\0';
			handle_command(cmd_buffer, cmd_idx);
			current_state = STATE_PRINTING;
		} else {
			if (cmd_idx < sizeof(cmd_buffer) - 1) {
				cmd_buffer[cmd_idx++] = (char)byte;
			}
		}
	}
}

/**
 * @brief Main processing loop for Core 1 (high-priority printer thread).
 *
 * Runs an infinite polling loop reading incoming characters pushed by Core 0
 * through the hardware inter-processor SIO FIFO.
 */
void core1_entry() {
	printf("MSG:CORE1_STARTED_PRINTER_LOOP\n");

	while (true) {
		// Check if Core 0 pushed network or USB data into the hardware FIFO
		if (multicore_fifo_rvalid()) {
			uint32_t incoming_msg = multicore_fifo_pop_blocking();

			// Core 1 strictly processes FIFO messages tagged as DATA (Bit 31 = 0)
			if ((incoming_msg & 0x80000000) == FIFO_TAG_DATA) {
				process_byte((uint8_t)(incoming_msg & 0xFF));
			}
		}
		tight_loop_contents();
	}
}

/**
 * @brief LowIP TCP payload reception callback (executes in Core 0 context).
 *
 * Iterates through arriving TCP packet buffers (pbufs), acknowledges byte
 * consumption, and passes character bytes across the inter-core FIFO to Core 1.
 *
 * @param arg User state argument (unused).
 * @param tpcb The active TCP protocol control block.
 * @param p Pointer to linked list of payload buffers.
 * @param err LWIP error code.
 * @return err_t ERR_OK upon completion.
 */
static err_t tcp_recv_callback(void* arg, struct tcp_pcb* tpcb, struct pbuf* p,
	err_t err) {
	if (p != NULL) {
		// Inform TCP stack that bytes have been accepted into application layer
		tcp_recved(tpcb, p->tot_len);

		// Traverse linked list of network buffers
		for (struct pbuf* q = p; q != NULL; q = q->next) {
			uint8_t* src = (uint8_t*)q->payload;
			for (int i = 0; i < q->len; i++) {
				// Safely transfer incoming byte across SIO FIFO to Core 1
				send_print_byte_to_core1(src[i]);
			}
		}
		pbuf_free(p); // Release packet memory back to LWIP memory pool
	} else if (err == ERR_OK) {
		// Remote host closed TCP connection cleanly
		tcp_close(tpcb);
	}
	return ERR_OK;
}

/**
 * @brief LowIP TCP incoming connection acceptance callback (on Core 0).
 *
 * Binds the receive callback function to newly established incoming client
 * connections.
 *
 * @param arg User state argument (unused).
 * @param newpcb Newly formed connection TCP protocol control block.
 * @param err LWIP error code.
 * @return err_t ERR_OK on success.
 */
static err_t tcp_accept_callback(void* arg, struct tcp_pcb* newpcb, err_t err) {
	if (err == ERR_OK && newpcb != NULL) {
		tcp_recv(newpcb, tcp_recv_callback);
	}
	return ERR_OK;
}

/**
 * @brief Processes incoming FIFO messages from Core 1 on Core 0.
 */
void process_fifo_message(uint32_t msg) {
	uint32_t msg_type = msg & FIFO_MSG_MASK_TYPE;
	uint32_t payload = msg & FIFO_MSG_MASK_DATA;

	switch (msg_type) {
		case FIFO_MSG_JOB_START:
			system_status = STATUS_PRINTING;
			display_status(STATUS_PRINTING, 0);
			break;
		case FIFO_MSG_BYTE_COUNT:
			display_status(STATUS_PRINTING, payload);
			break;
		case FIFO_MSG_JOB_END:
			system_status = STATUS_IDLE;
			display_status(STATUS_IDLE, payload);
			break;
		default:
			break;
	}
}

void poll_printer_status(void) {
	uint8_t new_err = check_for_error();

	// Only update if state actually changes
	if (new_err != current_err_state) {
		current_err_state = new_err;

		if (current_err_state != ERR_NONE) {
			system_status = STATUS_ERROR;
			display_status(STATUS_ERROR, current_err_state);
		} else {
			// Error cleared: restore previous state
			if (is_printing) {
				system_status = STATUS_PRINTING;
				display_status(STATUS_PRINTING, total_job_bytes);
			} else {
				system_status = STATUS_IDLE;
				display_status(STATUS_IDLE, 0);
			}
		}
	}
}

uint8_t check_for_error(void) {
	uint8_t err_state = ERR_NONE;
	// Check SELECT, /ERROR and PE signals, in that order
	if (gpio_get(SELECT_PIN) == 0) {	// Printer is offline
		err_state = ERR_OFFLINE;
	}

	if (gpio_get(ERROR_PIN) == 0) {		// Printer is indicating an error
		err_state = ERR_GEN;
	}

	if (gpio_get(PAPER_END_PIN) == 1) {	// Paper end condition
		err_state = ERR_PE;
	}
	return err_state;
}

// ============================================================================
// MAIN PROGRAM ENTRY POINT (CORE 0)
// ============================================================================

/**
 * @brief Main entry point running on Core 0.
 *
 * Initialises stdio, wifi drivers, establishes network association,
 * configures the RAW TCP port 9100 server, and launches Core 1 before
 * entering the CDC polling loop.
 *
 * @return int Standard exit code (never reached).
 */
int main() {
	stdio_init_all();
	sleep_ms(3000); // USB CDC serial console settling delay

	// Initialise hardware lines and I2C peripheral first
	init_hardware();
	init_display();

	snprintf(ip_buf, sizeof(ip_buf), "IP: %s", "0.0.0.0");

	printf("\nMSG:SMARTMATRIX_INITIALISED_Core_0\n");

	// Initialise the CYW43439 wifi hardware stack FIRST
	printf("MSG:INITIALISING_WIFI_CHIP\n");
	if (cyw43_arch_init_with_country(CYW43_COUNTRY('F', 'R', 0))) {
		printf("ERR:CYW43_INIT_FAILED\n");
		return -1;
	}

	// Enable station (client) mode
	cyw43_arch_enable_sta_mode();

	struct netif* netif = &cyw43_state.netif[CYW43_ITF_STA];
	netif_set_hostname(netif, NET_IF_HOSTNAME);

	printf("MSG:WIFI_CONNECTING_SSID_%s\n", WIFI_SSID);

	// Disable power-save sleep modes to optimise DHCP & TCP negotiation speed
	cyw43_wifi_pm(&cyw43_state, CYW43_NO_POWERSAVE_MODE);

	// Wifi association retry loop
	// Resolves AP session state drop delays (ERR_TIMEOUT -8 on soft resets)
	int err = -1;
	int retries = 0;

	while (err != 0 && retries < WIFI_MAX_TRIES) {
		retries++;
		char boot_msg[24];
		snprintf(boot_msg, sizeof(boot_msg), "CONNECTING (%d/5)...", retries);
		display_activity("WIFI", boot_msg, "0.0.0.0");
		if (retries > 1) {
			printf("MSG:WIFI_RETRY_ATTEMPT_%d/%d\n", retries, WIFI_MAX_TRIES);
			gpio_put(WIFI_LED_PIN, 0);
			sleep_ms(2000); // Delay to allow AP state machine reset
		}
		gpio_put(WIFI_LED_PIN, 1); // On to indicate attempt to connect

		err = cyw43_arch_wifi_connect_timeout_ms(
			WIFI_SSID,
			WIFI_PASSWORD,
			CYW43_AUTH_WPA2_AES_PSK,
			15000 // 15s timeout window per attempt
		);
	}

	if (err != 0) {
		printf("ERR:FATAL_WIFI_CONNECTION_FAILED_%d\n", err);
		display_activity("WIFI", "NO IP", "WIFI FAILED");
		gpio_put(WIFI_LED_PIN, 0); // Off to show failure to connect
		return -1;
	}

	snprintf(ip_buf, sizeof(ip_buf), "IP: %s", ip4addr_ntoa(netif_ip4_addr(netif)));
	printf("MSG:WIFI_CONNECTED\n");
	printf("MSG:IP_ADDRESS_%s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
	display_status(STATUS_IDLE, 0);

	// Flash LED to show success
	for (uint8_t i = 0; i < 5; i++) {
		sleep_ms(100);
		gpio_put(WIFI_LED_PIN, 0);
		sleep_ms(100);
		gpio_put(WIFI_LED_PIN, 1);
	}

	// Initialise RAW TCP Port 9100 Server using background thread-safe locks
	cyw43_arch_lwip_begin();
	struct tcp_pcb* pcb = tcp_new();
	if (pcb != NULL) {
		if (tcp_bind(pcb, IP_ADDR_ANY, TCP_PORT) == ERR_OK) {
			pcb = tcp_listen(pcb);
			tcp_accept(pcb, tcp_accept_callback);
			printf("MSG:TCP_SERVER_LISTENING\n");
		}
	}
	cyw43_arch_lwip_end();

	// Launch dedicated printer worker core (Core 1)
	multicore_launch_core1(core1_entry);

	// Core 0 execution loop: handles USB CDC serial backup input and FIFO msgs
	while (true) {
		// Non-blocking poll for local USB Serial terminal bytes
		int usb_char = getchar_timeout_us(0);
		if (usb_char != PICO_ERROR_TIMEOUT) {
			// Safely forward local USB inputs to Core 1 over FIFO
			send_print_byte_to_core1((uint8_t)usb_char);
		}

		// FIFO message processing from Core 1
		if (multicore_fifo_rvalid()) {
			uint32_t msg = multicore_fifo_pop_blocking();

			// Core 0 processes FIFO messages tagged as messages (Bit 31 = 1)
			if ((msg & 0x80000000) == FIFO_TAG_MSG) {
				process_fifo_message(msg);
			}
		}

		poll_printer_status();

		sleep_ms(1); // Yield execution slot
	}

	return 0;
}
