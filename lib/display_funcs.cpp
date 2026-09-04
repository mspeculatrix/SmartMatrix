#include "display_funcs.h"

/**
 * @brief Initialises the SSD1306 display over I2C
 */
void init_display() {
	ssd1306_init(&display, I2C_PORT);			// Device init
	// Clear display then show boot message
	ssd1306_clear(&display);
	ssd1306_printstr_double(&display, 0, 0, "BOOTING...");
	ssd1306_println(&display, 3, "SMARTMATRIX");
	ssd1306_println(&display, 5, VERSION_STR);
	ssd1306_show(&display);
}

/**
 * @brief Renders network and job status onto the OLED screen (Core 0)
 */
void display_activity(const char* header, const char* activity,
	const char* status) {
	ssd1306_clear(&display);
	ssd1306_printstr_double(&display, 0, 0, header);
	ssd1306_println(&display, 3, activity);
	ssd1306_println(&display, 7, status);
	ssd1306_show(&display);
}

/**
 * @brief Prints status info
 * @param SystemStatus Status code
 * @param data Integer: byte count or error code
 */
void display_status(SystemStatus state, uint32_t data) {
	ssd1306_clear(&display);
	bool show_bytes = false;

	if (state == STATUS_PRINTING) {
		ssd1306_printstr_double(&display, 0, 0, "PRINTING");
		show_bytes = true;
	} else if (state == STATUS_IDLE) {
		ssd1306_printstr_double(&display, 0, 0, "IDLE/READY");
		show_bytes = true;
	} else if (state == STATUS_ERROR) {
		// ErrorState code is passed in the data parameter
		ssd1306_printstr_double(&display, 0, 0, error_msg[data]);
	}

	if (show_bytes) {
		// Number of bytes is passed in the data parameter
		ssd1306_println(&display, 3, "BYTES:");
		char count_buf[10];
		snprintf(count_buf, sizeof(count_buf), "%lu", (unsigned long)data);
		ssd1306_printstr_double(&display, 48, 24, count_buf);
	}

	display_AF();						// Autofeed setting
	display_SSID();						// Wifi SSID currently configured
	display_IP();						// Current IP address

	ssd1306_show(&display);
}

/**
 * @brief Display current Autofeed setting
 */
void display_AF(void) {
	char stat_msg[9];
	if (autofeed_cfg == AF_ON) {
		snprintf(stat_msg, sizeof(stat_msg), "%s", "AUTOFEED");
	} else {
		snprintf(stat_msg, sizeof(stat_msg), "%s", "AF off");
	}
	ssd1306_println(&display, 5, stat_msg);
	ssd1306_show(&display);
}

/**
 * @brief Display current IP address
 */
void display_IP(void) {
	if (wifi_connected) {
		ssd1306_println(&display, 7, ip_buf);
	} else {
		ssd1306_println(&display, 7, "NO WIFI");
	}
	ssd1306_show(&display);
}

/**
 * @brief Display currently configured SSID
 */
void display_SSID(void) {
	ssd1306_println(&display, 6, wifi_ssid);
	ssd1306_show(&display);
}
