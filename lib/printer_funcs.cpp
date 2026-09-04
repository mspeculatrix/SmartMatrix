#include "printer_funcs.h"

/**
 * @brief Check signals from printer for any error states
 * @return ErrorState error code
 *
 * Checks /ERROR, PE and SELECT signals, in that order.
 */
ErrorState check_for_error(void) {
	ErrorState err_state = ERR_NONE;

	if (gpio_get(ERROR_PIN) == 0) {			// Printer is indicating an error
		err_state = ERR_GEN;
	}
	if (gpio_get(PAPER_END_PIN) == 1) {		// Paper end condition
		err_state = ERR_PE;
	}
	if (gpio_get(SELECT_PIN) == 0) {		// Printer is offline
		err_state = ERR_OFFLINE;
	}
	return err_state;
}

/**
 * @brief Polls hardware pins and manages state transitions on Core 0.
 *
 * State changes now persist correctly, protecting STATUS_PRINTING from
 * being cleared on every loop iteration.
 */
void poll_printer_status(void) {
	ErrorState new_err = check_for_error();

	// Update state only if error status actually changes
	if (new_err != current_err_state) {
		current_err_state = new_err;

		if (current_err_state != ERR_NONE) {
			system_status = STATUS_ERROR;
			display_status(STATUS_ERROR, current_err_state);
		} else {
			// Error cleared: restore printing or idle state based on active
			// job flag
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

/**
 * @brief Transmits a single byte to the printer using Centronics handshake.
 *
 * Checks the printer BUSY line, places the data byte on the bus, and asserts
 * the /STROBE line for the required timing duration.
 *
 * @param character The 8-bit ASCII or binary byte to send.
 */
void send_byte_to_printer(uint8_t character) {
	// Light up LED to indicate activity
	gpio_put(ACTIVITY_LED_PIN, 1);

	/* Block until printer is ready. Signal conditions must be:
		BUSY   - LOW
		SELECT - HIGH
		/ERROR - HIGH
		PE     - LOW
	*/
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
