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
#include "__wifi_creds.h" // Wifi credentials (WIFI_SSID, WIFI_PASSWORD)

 // ============================================================================
 // CONFIGURATION CONSTANTS
 // ============================================================================

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
#define I2C_PORT        i2c0
#define I2C_SDA_PIN     20  // Note: can't use GPIO 16 = CYW43 Power Enable
#define I2C_SCL_PIN     21  // Note: can't use GPIO 17 (CYW43 conflict)
#define LED1_PIN        18  // Core activity indicator
#define LED2_PIN        19  // Character transmission toggle indicator

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

/** @brief In-band command accumulation buffer */
char cmd_buffer[256];

/** @brief Current index within the command buffer */
size_t cmd_idx = 0;

// =============================================================================
// FUNCTION IMPLEMENTATIONS
// =============================================================================

/**
 * @brief Configures GPIO directional modes, pull-up/downs, and I2C peripherals.
 *
 * Initialises GPIO 0-7 for character data, output/input control lines for the
 * printer interface, and binds I2C0 to GPIO 20/21.
 *
 * @note DESIGN CHOICE: Called strictly AFTER wifi initialisation succeeds to
 * ensure no I/O pin-state transients disrupt CYW43 radio initialisation.
 */
void init_hardware() {
	// Data Bus (GPIO 0-7) configured as outputs
	for (int i = 0; i < 8; i++) {
		gpio_init(i);
		gpio_set_dir(i, GPIO_OUT);
	}

	// IEEE 1284 output control lines & status LEDs
	const uint32_t outputs[] = { STROBE_PIN, INIT_PIN, AUTOFEED_PIN, LED1_PIN, LED2_PIN };
	for (uint32_t pin : outputs) {
		gpio_init(pin);
		gpio_set_dir(pin, GPIO_OUT);
	}

	// Set default idle signal states (Centronics active-low idle voltages)
	gpio_put(STROBE_PIN, 1);   // Idle high
	gpio_put(INIT_PIN, 1);     // Idle high (no reset)
	gpio_put(AUTOFEED_PIN, 1); // Active low; Disabled by default
	gpio_put(LED1_PIN, 1);     // On, because why not?
	gpio_put(LED2_PIN, 0);     // Data TX LED OFF

	// IEEE 1284 input status lines
	const uint32_t inputs[] = { BUSY_PIN, ACK_PIN, PAPER_END_PIN, SELECT_PIN, ERROR_PIN };
	for (uint32_t pin : inputs) {
		gpio_init(pin);
		gpio_set_dir(pin, GPIO_IN);
		gpio_disable_pulls(pin); // We have external pull-ups on the PCB
	}

	// Auxiliary I2C setup (for OLED, one day)
	i2c_init(I2C_PORT, 400 * 1000);
	gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
	gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(I2C_SDA_PIN);
	gpio_pull_up(I2C_SCL_PIN);
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
	// Toggle transmission LED to indicate active bit-banging activity
	gpio_put(LED2_PIN, !gpio_get(LED2_PIN));

	// Wait blocking until the printer signals it is ready to receive data
	while (gpio_get(BUSY_PIN)) {
		tight_loop_contents(); // Microcontroller spinlock hint
	}

	// Write byte to lower 8 pins (GPIO 0-7) using atomic bit-masking
	gpio_put_masked(DATA_MASK, (uint32_t)character);

	// IEEE 1284 strobe pulse setup time
	sleep_us(HOLD_DURATION);
	gpio_put(STROBE_PIN, 0); // Assert STROBE (active low)
	sleep_us(STROBE_DURATION);
	gpio_put(STROBE_PIN, 1); // De-assert STROBE
	sleep_us(HOLD_DURATION); // Hold data post-edge
}

/**
 * @brief Processes in-band ASCII control commands received over the stream.
 *
 * @param buffer Pointer to the null-terminated command string.
 * @param length Length of the string in bytes.
 */
void handle_command(const char* buffer, size_t length) {
	if (strncmp(buffer, "STATUS", length) == 0) {
		// Sample physical status input pins
		bool pe = gpio_get(PAPER_END_PIN);
		bool err = !gpio_get(ERROR_PIN); // Error pin is active low

		if (pe) {
			printf("ERR:PAPER_OUT\n");
		} else if (err) {
			printf("ERR:PRINTER_FAULT\n");
		} else {
			printf("MSG:PRINTER_READY\n");
		}
	} else if (strncmp(buffer, "RESET", length) == 0) {
		// Assert hardware initialise pulse
		gpio_put(INIT_PIN, 0);
		sleep_ms(RESET_DURATION);
		gpio_put(INIT_PIN, 1);
		printf("MSG:PRINTER_RESET_EXECUTED\n");
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
		} else if (byte >= 0x07) { // Suppress low non-printable bytes
			send_byte_to_printer(byte);
		}
	} else if (current_state == STATE_COMMAND) {
		if (byte == CMD_MODE_END) {
			cmd_buffer[cmd_idx] = '\0'; // Null-terminate collected command
			handle_command(cmd_buffer, cmd_idx);
			current_state = STATE_PRINTING; // Return to pass-through mode
		} else {
			// Buffer safety overflow protection
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
			uint32_t incoming_char = multicore_fifo_pop_blocking();
			process_byte((uint8_t)incoming_char);
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
				multicore_fifo_push_blocking((uint32_t)src[i]);
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
		if (retries > 1) {
			printf("MSG:WIFI_RETRY_ATTEMPT_%d/%d\n", retries, WIFI_MAX_TRIES);
			sleep_ms(2000); // Delay to allow AP state machine reset
		}

		err = cyw43_arch_wifi_connect_timeout_ms(
			WIFI_SSID,
			WIFI_PASSWORD,
			CYW43_AUTH_WPA2_AES_PSK,
			15000 // 15s timeout window per attempt
		);
	}

	if (err != 0) {
		printf("ERR:FATAL_WIFI_CONNECTION_FAILED_%d\n", err);
		return -1;
	}

	printf("MSG:WIFI_CONNECTED\n");
	printf("MSG:IP_ADDRESS_%s\n", ip4addr_ntoa(netif_ip4_addr(netif)));

	// Initialise parallel printer IO hardware after wifi stack is fully up
	init_hardware();

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

	// Core 0 execution loop: handles USB CDC serial backup input
	while (true) {
		// Non-blocking poll for local USB Serial terminal bytes
		int usb_char = getchar_timeout_us(0);
		if (usb_char != PICO_ERROR_TIMEOUT) {
			// Safely forward local USB inputs to Core 1 over FIFO
			multicore_fifo_push_blocking((uint32_t)usb_char);
		}

		sleep_ms(1); // Yield execution slot
	}

	return 0;
}
