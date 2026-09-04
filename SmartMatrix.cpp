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
#include "lib/defines.h"
#include "lib/pico_ssd1306_basic.h"
#include "lib/display_funcs.h"
#include "lib/printer_funcs.h"
#include "lib/serial_funcs.h"
#include "lib/wifi_funcs.h"
#include "lib/wifi_creds.h"

SystemContext* sys_ctx = new SystemContext{};
NetworkContext* net_ctx = new NetworkContext{};

/// @brief OLED display
ssd1306_t display;

// =============================================================================
// FUNCTION PROTOTYPES
// =============================================================================

int app_init(void);
void core1_entry();
void init_hardware();

// TCP functions
static err_t tcp_accept_callback(void* arg, struct tcp_pcb* newpcb, err_t err);
static err_t tcp_recv_callback(void* arg, struct tcp_pcb* tpcb,
	struct pbuf* p, err_t err);

// *****************************************************************************
// FUNCTION IMPLEMENTATIONS
// *****************************************************************************

int app_init(void) {
	if (sys_ctx && net_ctx) {
		// Initialize system context fields
		sys_ctx->cmd_idx = 0;
		sys_ctx->system_status = STATUS_IDLE;
		sys_ctx->serial_mode = STATE_COMMAND;
		sys_ctx->current_err_state = ERR_NONE;
		sys_ctx->system_status_msg[0] = "IDLE/READY";
		sys_ctx->system_status_msg[1] = "PRINTING";
		sys_ctx->system_status_msg[2] = "ERROR";
		sys_ctx->error_msg[0] = "OK";
		sys_ctx->error_msg[0] = "OFFLINE";
		sys_ctx->error_msg[0] = "ERROR";
		sys_ctx->error_msg[0] = "PAPER OUT";
		sys_ctx->error_msg[0] = "NO WIFI";

		sys_ctx->autofeed_cfg = AF_OFF;
		sys_ctx->total_job_bytes = 0;
		sys_ctx->is_printing = false;

		// Initialize network context fields
		net_ctx->netif = &cyw43_state.netif[CYW43_ITF_STA];
		net_ctx->wifi_connected = false;
		snprintf(net_ctx->ip_buf, sizeof(net_ctx->ip_buf), "--");
		strncpy(net_ctx->wifi_ssid, WIFI_SSID, sizeof(net_ctx->wifi_ssid) - 1);
		// Ensure null-termination
		net_ctx->wifi_ssid[sizeof(net_ctx->wifi_ssid) - 1] = '\0';
		strncpy(net_ctx->wifi_passwd, WIFI_PASSWORD,
			sizeof(net_ctx->wifi_passwd) - 1);
		// Ensure null-termination
		net_ctx->wifi_passwd[sizeof(net_ctx->wifi_passwd) - 1] = '\0';

	} else {
		printf("! ERR: App init failed\n");
		return -1;
	}
	return 0;
}

/**
 * @brief Main processing loop for Core 1 (high-priority printer thread).
 *
 * Runs an infinite polling loop reading incoming characters pushed by Core 0
 * through the hardware inter-processor SIO FIFO.
 */
void core1_entry() {
	printf(": STARTED PRINTER LOOP (Core 1)\n");
	printf("\nType HELP for a list of available commands.\n");
	printf("\nREADY\n");

	while (true) {
		// Check if Core 0 pushed network or USB data into the hardware FIFO
		if (multicore_fifo_rvalid()) {
			uint32_t incoming_msg = multicore_fifo_pop_blocking();

			// Core 1 processes only FIFO messages tagged as DATA (Bit 31 = 0)
			if ((incoming_msg & 0x80000000) == FIFO_TAG_DATA) {
				uint8_t print_byte = (uint8_t)(incoming_msg & 0xFF);

				// Manage job statistics and status messaging on Core 1
				// direct execution
				if (!sys_ctx->is_printing) {
					sys_ctx->is_printing = true;
					sys_ctx->total_job_bytes = 0;
					send_fifo_msg(FIFO_MSG_JOB_START, 0);
				}

				// Send byte directly to hardware pins via Centronics handshake
				send_byte_to_printer(print_byte);
				sys_ctx->total_job_bytes++;

				// Notify Core 0 every 100 bytes to update the OLED display
				if (sys_ctx->total_job_bytes % 100 == 0) {
					send_fifo_msg(FIFO_MSG_BYTE_COUNT,
						sys_ctx->total_job_bytes);
				}
			}
		} else {
			// If FIFO stays empty and we were printing, mark end of job
			if (sys_ctx->is_printing) {
				// Short settling delay before declaring job finished
				sleep_ms(500);
				if (!multicore_fifo_rvalid()) {
					sys_ctx->is_printing = false;
					send_fifo_msg(FIFO_MSG_JOB_END, sys_ctx->total_job_bytes);
				}
			}
		}
		tight_loop_contents();
	}
}

/**
 * @brief Configures GPIO directional modes, pull-up/downs, and I2C interface.
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
	const uint32_t outputs[] = {
		STROBE_PIN,
		INIT_PIN,
		AUTOFEED_PIN,
		ACTIVITY_LED_PIN,
		WIFI_LED_PIN };

	for (uint32_t pin : outputs) {
		gpio_init(pin);
		gpio_set_dir(pin, GPIO_OUT);
	}

	// Set default idle signal states (Centronics active-low idle voltages)
	gpio_put(STROBE_PIN, 1);        // Idle high
	gpio_put(INIT_PIN, 1);          // Idle high (no reset)
	gpio_put(AUTOFEED_PIN, sys_ctx->autofeed_cfg);
	gpio_put(ACTIVITY_LED_PIN, 1);  // On, but probably not long enough to see
	gpio_put(WIFI_LED_PIN, 0);      // Off by default

	// IEEE 1284 input status lines
	const uint32_t inputs[] = {
		BUSY_PIN,
		ACK_PIN,
		PAPER_END_PIN,
		SELECT_PIN,
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
	//gpio_pull_up(I2C_SDA_PIN); 	// SmartMatrix board has hardware pullups
	//gpio_pull_up(I2C_SCL_PIN); 	// for I2C, so don't need these lines.
	gpio_put(ACTIVITY_LED_PIN, 0);
}

// -----------------------------------------------------------------------------
// TCP FUNCTIONS
// -----------------------------------------------------------------------------

/**
 * @brief LowIP TCP payload reception callback (executes in Core 0 context).
 *
 * Iterates through arriving TCP packet buffers (pbufs), acknowledges byte
 * consumption, and passes character bytes across the inter-core FIFO to Core 1.
 *
 * @param arg User state argument (unused).
 * @param tpcb The active TCP protocol control block.
 * @param bufList Pointer to linked list of payload buffers.
 * @param err LWIP error code.
 * @return err_t ERR_OK upon completion.
 */
static err_t tcp_recv_callback(void* arg,
	struct tcp_pcb* tpcb,
	struct pbuf* bufList,
	err_t err) {
	if (bufList != NULL) {
		// Inform TCP stack that bytes have been accepted into application layer
		tcp_recved(tpcb, bufList->tot_len);

		// Traverse linked list of network buffers
		for (struct pbuf* q = bufList; q != NULL; q = q->next) {
			uint8_t* src = (uint8_t*)q->payload;
			for (int i = 0; i < q->len; i++) {
				// TCP is ALWAYS in print mode. Transfer directly to
				// Core 1 FIFO.
				send_print_byte_to_core1(src[i]);
			}
		}
		pbuf_free(bufList); // Release packet memory back to LWIP memory pool
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
		// Refuse incoming TCP print connections if hardware printer is in an
		// error state
		if (sys_ctx->system_status == STATUS_ERROR) {
			printf("! TCP REJECTED: PRINTER IN ERROR STATE\n");
			tcp_abort(newpcb);
			return ERR_ABRT;
		}
		// Otherwise, grab the byte and pass the callback function to use
		tcp_recv(newpcb, tcp_recv_callback);
	}
	return ERR_OK;
}

// *****************************************************************************
// MAIN PROGRAM ENTRY POINT (CORE 0)
// *****************************************************************************

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
	app_init(); 		// Set up global contexts
	init_hardware();
	init_display();
	snprintf(net_ctx->ip_buf, sizeof(net_ctx->ip_buf), "IP: %s", "--");

	printf("\n: SMARTMATRIX INITIALISED (Core 0)\n");

	// ----- WIFI --------------------------------------------------------------

	net_ctx->wifi_connected = false;					// Reset

	// Initialise the CYW43439 wifi hardware stack first
	printf(": Initialising WIFI system\n");
	// if (cyw43_arch_init_with_country(CYW43_COUNTRY('F', 'R', 0))) {
	if (cyw43_arch_init_with_country(CYW43_COUNTRY_WORLDWIDE)) {
		// Returns an error code if failed, 0 if successful
		printf("! ERR: CYW43 INIT FAILED\n");

	} else {										// Successful connection
		// Enable station (client) mode
		cyw43_arch_enable_sta_mode();
		// Disable power-save modes to optimise DHCP & TCP negotiation speed
		cyw43_wifi_pm(&cyw43_state, CYW43_NO_POWERSAVE_MODE);

		netif_set_hostname(net_ctx->netif, NET_IF_HOSTNAME);

		// Try reading the wifi credentials from non-volatile flash memory
		bool wifi_configured = load_wifi_credentials(net_ctx);
		if (wifi_configured) {	// Creds have been stored in flash already
			printf(": WIFI is configured\n");
			if (wifi_connect(net_ctx) == 0) {
				net_ctx->wifi_connected = true;
				display_status(sys_ctx, net_ctx, STATUS_IDLE, 0); // Default
			}
		} else {
			// No stored creds. Let's see what was read in from the wifi_creds.h
			// file during compilation.
			// Check the SSID setting (not password because user might be using
			// an open wifi setup).
			if (strlen(net_ctx->wifi_ssid) > 0) {
				// We have something, so attempt a connection
				if (wifi_connect(net_ctx) == 0) {
					// Successful - save to non-volatile memory for next time
					save_wifi_credentials(net_ctx);
					net_ctx->wifi_connected = true;
				}
			}
		}
	}

	if (!net_ctx->wifi_connected) {
		display_status(sys_ctx, net_ctx, STATUS_ERROR, ERR_WIFI);
	}

	// ----- TCP SOCKET SERVER -------------------------------------------------

	// Initialise RAW TCP Port 9100 Server using background thread-safe locks
	cyw43_arch_lwip_begin();
	struct tcp_pcb* pcb = tcp_new();
	if (pcb != NULL) {
		if (tcp_bind(pcb, IP_ADDR_ANY, TCP_PORT) == ERR_OK) {
			pcb = tcp_listen(pcb);
			tcp_accept(pcb, tcp_accept_callback);
			printf(": TCP server listening\n");
		}
	}
	cyw43_arch_lwip_end();

	// ----- MAIN LOOPS --------------------------------------------------------

	// Launch dedicated printer worker core (Core 1)
	multicore_launch_core1(core1_entry);

	// Core 0 execution loop: handles USB CDC serial backup input and FIFO msgs
	while (true) {
		// Non-blocking poll for local USB Serial terminal bytes
		int usb_char = getchar_timeout_us(0);
		if (usb_char != PICO_ERROR_TIMEOUT) {
			// Pass incoming USB bytes to process_usb_byte on Core 0 context
			process_usb_byte(sys_ctx, net_ctx, (uint8_t)usb_char);
		}

		// FIFO message processing from Core 1
		if (multicore_fifo_rvalid()) {
			uint32_t msg = multicore_fifo_pop_blocking();

			// Core 0 processes FIFO messages tagged as messages (Bit 31 = 1)
			if ((msg & 0x80000000) == FIFO_TAG_MSG) {
				process_fifo_message(sys_ctx, net_ctx, msg);
			}
		}

		poll_printer_status(sys_ctx, net_ctx);		// Check for error states

		sleep_ms(1); 								// Yield execution slot
	}

	return 0;
}
