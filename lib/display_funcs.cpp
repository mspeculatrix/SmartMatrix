#include "display_funcs.h"

extern ssd1306_t display;
extern uint8_t autofeed_cfg;
extern char ip_buf[20];
extern const char* error_msg[];
extern char wifi_ssid[];
extern bool wifi_connected;

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
void display_activity(const char* header, const char* activity,
	const char* status) {
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
		ssd1306_printstr_double(&display, 0, 0, error_msg[data]);
	}

	if (show_bytes) {
		ssd1306_println(&display, 3, "BYTES:");
		char count_buf[10];
		snprintf(count_buf, sizeof(count_buf), "%lu", (unsigned long)data);
		ssd1306_printstr_double(&display, 48, 24, count_buf);
	}

	display_AF();
	display_SSID();
	display_IP();

	ssd1306_show(&display);
}

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

void display_IP(void) {
	if (wifi_connected) {
		ssd1306_println(&display, 7, ip_buf);
	} else {
		ssd1306_println(&display, 7, "NO WIFI");
	}
	ssd1306_show(&display);
}

void display_SSID(void) {
	ssd1306_println(&display, 6, wifi_ssid);
	ssd1306_show(&display);
}
