#include "serial_funcs.h"

/**
 * @brief Processes in-band ASCII control commands received over the stream.
 *
 * @param buffer Pointer to the null-terminated command string.
 * @param length Length of the string in bytes.
 */
void handle_command(SystemContext* sys, NetworkContext* net) {

	// The first time through, the string passed in the buffer is assumed
	// to be a command. We make command_mode static because, in some cases,
	// we'll want to change it to something else in order to parse
	// parameters.
	static CommandState command_mode = CMD_COMMAND;

	size_t length = sizeof(sys->cmd_buffer);

	switch (command_mode) {

		case CMD_COMMAND:
			if (strncmp(sys->cmd_buffer, "STAT", length) == 0) {
				// Status report
				printf("\nSMARTMATRIX %s\n", VERSION_STR);
				if (sys->system_status != STATUS_ERROR) {
					printf(sys->system_status_msg[sys->system_status]);
				} else {
					printf(sys->error_msg[sys->current_err_state]);
				}
				printf("\n");

				if (gpio_get(SELECT_PIN) == 0) {		// Printer is offline
					printf("- Printer offline\n");
				} else {
					printf("- Printer online\n");
				}

				if (gpio_get(ERROR_PIN) == 0) {	// Printer indicates an error
					printf("- Printer indicating an error\n");
				}

				if (gpio_get(PAPER_END_PIN) == 1) {		// Paper end condition
					printf("- Printer indicating paper out\n");
				}
				printf("Autofeed ");

				if (sys->autofeed_cfg == AF_ON) {
					printf("ON\n");
				} else {
					printf("OFF\n");
				}

				printf("Wifi SSID: %s\n", net->wifi_ssid);
				printf("Wifi password ");
				if (strlen(net->wifi_passwd) == 0) {
					printf("not ");
				}
				printf("configured\n");
				printf("IP: %s\n", net->ip_buf);

			} else if (strncmp(sys->cmd_buffer, "HELP", length) == 0) {
				// Request for help message
				printf("\nSMARTMATRIX %s\n", VERSION_STR);
				printf("---------------------------------\n");
				printf("PRT     Start direct print mode\n");
				printf("STAT    Get status\n");
				printf("AF_ON   Enable Autofeed function\n");
				printf("AF_OFF  Disable Autofeed function\n");
				printf("\n--- WIFI ---\n");
				printf("CONN    Initiate Wifi connection\n");
				printf("PASSWD  Enter Wifi password\n");
				printf("SSID    Enter Wifi SSID\n");
				printf("\n");

			} else if (strncmp(sys->cmd_buffer, "RESET", length) == 0) {
				// Request to reset the printer (not the SmartMatrix)
				if (sys->system_status == STATUS_IDLE) {
					printf(": Resetting... ");
					gpio_put(ACTIVITY_LED_PIN, 1);
					gpio_put(INIT_PIN, 0);
					sleep_ms(RESET_DURATION);
					gpio_put(INIT_PIN, 1);
					gpio_put(ACTIVITY_LED_PIN, 0);
					printf("PRINTER RESET\n");
				} else {
					printf("! ERR: Printer busy or offline.\n");
				}

			} else if (strncmp(sys->cmd_buffer, "PRT", length) == 0) {
				// Switch into serial printing mode
				if (sys->system_status == STATUS_IDLE) {
					sys->serial_mode = STATE_PRINTING;
					printf("RDY\n");                 // Acknowledgement message
				} else {
					printf("! ERR: Printer not available.\n");
				}

			} else if (strncmp(sys->cmd_buffer, "CONN", length) == 0) {
				// Connect to wifi
				net->wifi_connected = false;					// Reset
				if (wifi_connect(net) == 0) {
					net->wifi_connected = true;
					display_status(sys, net, STATUS_IDLE, 0);
				} else {
					display_status(sys, net, STATUS_ERROR, ERR_WIFI);
					sleep_ms(3000); // Give user a chance to read the message
				}

			} else if (strncmp(sys->cmd_buffer, "IP", length) == 0) {
				printf("IP: %s\n", net->ip_buf);

			} else if (strncmp(sys->cmd_buffer, "SSID", length) == 0) {
				// Request to set SSID for wifi. The password will be
				// assumed to be the next string processed by this function.
				command_mode = CMD_SSID;
				printf("> ENTER WIFI SSID: ");        // Prompt

			} else if (strncmp(sys->cmd_buffer, "PASSWD", length) == 0) {
				// Request to set password for wifi. The password will be
				// assumed to be the next string processed by this function.
				command_mode = CMD_PASSWD;
				printf("> ENTER WIFI PASSWORD: ");    // Prompt

			} else if (strncmp(sys->cmd_buffer, "AF_ON", length) == 0) {
				// Request to turn Autofeed on
				printf(": AUTOFEED ON\n");
				sys->autofeed_cfg = AF_ON;
				gpio_put(AUTOFEED_PIN, sys->autofeed_cfg);
				display_AF(sys);

			} else if (strncmp(sys->cmd_buffer, "AF_OFF", length) == 0) {
				// Request to turn Autofeed off
				printf(": AUTOFEED OFF\n");
				sys->autofeed_cfg = AF_OFF;
				display_AF(sys);
				gpio_put(AUTOFEED_PIN, sys->autofeed_cfg);

			} else {
				// Nothing matched, so...
				printf("! ERR: UNKNOWN COMMAND\n");
			}
			break;

		case CMD_SSID:
			// On this pass, the string is assumed to be the wifi SSID
			strncpy(net->wifi_ssid, sys->cmd_buffer, sizeof(net->wifi_ssid) - 1);
			net->wifi_ssid[sizeof(net->wifi_ssid) - 1] = '\0'; // Ensure null
			printf(": SSID set to: %s\n", net->wifi_ssid);
			save_wifi_credentials(net); 		// Persist update
			command_mode = CMD_COMMAND;			// Switch back to default mode
			break;
			break;

		case CMD_PASSWD:
			// On this pass, the string is assumed to be the wifi password
			strncpy(net->wifi_passwd, sys->cmd_buffer, sizeof(net->wifi_passwd) - 1);
			net->wifi_passwd[sizeof(net->wifi_passwd) - 1] = '\0';
			printf(": Password updated.\n");
			save_wifi_credentials(net); 		// Persist update
			command_mode = CMD_COMMAND;			// Switch back to default mode
			break;

		default:
			// For now just do nothing. We should never get here.
			printf("! ERR: Unexpected command mode!\n");
			break;
	}
	sys->cmd_idx = 0;
}


/**
 * @brief Processes incoming FIFO messages from Core 1 on Core 0.
 */
void process_fifo_message(SystemContext* sys,
	NetworkContext* net,
	uint32_t msg) {

	uint32_t msg_type = msg & FIFO_MSG_MASK_TYPE;
	uint32_t payload = msg & FIFO_MSG_MASK_DATA;

	switch (msg_type) {
		case FIFO_MSG_JOB_START:
			sys->system_status = STATUS_PRINTING;
			display_status(sys, net, STATUS_PRINTING, 0);
			break;
		case FIFO_MSG_BYTE_COUNT:
			display_status(sys, net, STATUS_PRINTING, payload);
			break;
		case FIFO_MSG_JOB_END:
			sys->system_status = STATUS_IDLE;
			display_status(sys, net, STATUS_IDLE, payload);
			break;
		default:
			break;
	}
}


/**
 * @brief Evaluates bytes specifically from USB CDC (Serial) on Core 0.
 *
 * This function runs strictly in the Core 0 context to separate USB terminal
 * commands from binary network printing data.
 *
 * @param byte Incoming stream byte from USB CDC.
 */
void process_usb_byte(SystemContext* sys, NetworkContext* net, uint8_t byte) {
	// A byte has come in via the USB-serial port. Let's decide what to do
	// with it.

	// Are we currently in printing mode for the serial connection?
	if (sys->serial_mode == STATE_PRINTING) {

		if (byte == PRT_MODE_END) {    // Code indicating end of print mode
			sys->serial_mode = STATE_COMMAND;	// Switch back to normal mode
			sys->cmd_idx = 0;					// Reset command index
			printf("\n: EXITING PRINT MODE\n");
		} else {
			// Forward raw USB print byte to Core 1 via FIFO
			send_print_byte_to_core1(byte);
		}

		// } else if (sys->serial_mode == STATE_COMMAND) { 	// The normal state
	} else {
		// Otherwise we're in the normal STATE_COMMAND mode
		switch (byte) {
			case CHAR_CR:
				// Do nothing. We're going to ignore carriage returns
				break;
			case CHAR_LF:
				// This signals the end of a command.
				// Null terminate the string
				sys->cmd_buffer[sys->cmd_idx] = '\0';
				// Act on the command
				handle_command(sys, net);
				break;
			default:
				// Just add the character to the command buffer
				if (sys->cmd_idx < sizeof(sys->cmd_buffer) - 1) {
					sys->cmd_buffer[sys->cmd_idx++] = (char)byte;
				}
				break;
		}
	}
}
