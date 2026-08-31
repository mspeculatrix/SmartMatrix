#include "wifi_funcs.h"

/**
 * @brief Reads stored Wi-Fi credentials from the top Flash sector.
 * @return true if valid credentials exist; false otherwise.
 */
bool load_wifi_credentials(char* ssid_out, char* passwd_out) {
	const uint8_t* flash_target_contents = (const uint8_t*)(XIP_BASE + FLASH_CONFIG_OFFSET);
	const WifiCredentials* config = (const WifiCredentials*)flash_target_contents;

	// Must match magic header AND contain a non-empty SSID
	if (config->magic == WIFI_MAGIC_HEADER && strlen(config->ssid) > 0) {
		strncpy(ssid_out, config->ssid, WIFI_SSID_LEN + 1);
		strncpy(passwd_out, config->password, WIFI_PASS_LEN + 1);
		ssid_out[WIFI_SSID_LEN] = '\0';
		passwd_out[WIFI_PASS_LEN] = '\0';
		return true;
	}

	return false;
}

/**
 * @brief Erases flash sector and writes new Wi-Fi credentials.
 */
void save_wifi_credentials(const char* ssid, const char* passwd) {
	WifiCredentials config;
	memset(&config, 0, sizeof(config));

	config.magic = WIFI_MAGIC_HEADER;
	strncpy(config.ssid, ssid, WIFI_SSID_LEN);
	strncpy(config.password, passwd, WIFI_PASS_LEN);

	// Prepare buffer aligned to full page size (256 bytes)
	uint8_t buffer[FLASH_PAGE_SIZE];
	memset(buffer, 0xFF, sizeof(buffer)); // Default unprogrammed flash state
	memcpy(buffer, &config, sizeof(config));

	printf(": Saving Wi-Fi credentials to Flash...\n");

	// Disable interrupts to prevent flash execution crash while rewriting
	uint32_t ints = save_and_disable_interrupts();

	flash_range_erase(FLASH_CONFIG_OFFSET, FLASH_SECTOR_SIZE);
	flash_range_program(FLASH_CONFIG_OFFSET, buffer, FLASH_PAGE_SIZE);

	restore_interrupts(ints);

	printf(": Wi-Fi credentials saved successfully.\n");
}

int wifi_connect(char* ssid, char* passwd, struct netif* iface) {
	printf(": WIFI connecting to %s\n", ssid);

	int err = -1;
	int retries = 0;
	bool wifi_configured = load_wifi_credentials(ssid, passwd);
	if (wifi_configured) {
		while (err != 0 && retries < WIFI_MAX_TRIES) {
			retries++;
			char boot_msg[24];
			snprintf(boot_msg, sizeof(boot_msg), "CONNECTING (%d/5)...", retries);
			display_activity("WIFI", boot_msg, "awaiting connection");
			if (retries > 1) {
				printf(": WIFI retry attempt %d/%d\n", retries, WIFI_MAX_TRIES);
				gpio_put(WIFI_LED_PIN, 0);
				sleep_ms(2000);
			}
			gpio_put(WIFI_LED_PIN, 1);

			err = cyw43_arch_wifi_connect_timeout_ms(
				ssid,
				passwd,
				CYW43_AUTH_WPA2_AES_PSK,
				15000
			);
		}

		if (err != 0) {
			printf("! ERR: FATAL - WIFI CONNECTION FAILED %d\n", err);
			display_activity("WIFI", "NO IP", "WIFI FAILED");
			gpio_put(WIFI_LED_PIN, 0);
			return -1;
		}

		snprintf(ip_buf, sizeof(ip_buf), "IP: %s", ip4addr_ntoa(netif_ip4_addr(iface)));
		printf(": WIFI connected\n");
		printf(": IP address: %s\n", ip_buf);

		for (int i = 0; i < 5; i++) {
			sleep_ms(100);
			gpio_put(WIFI_LED_PIN, 0);
			sleep_ms(100);
			gpio_put(WIFI_LED_PIN, 1);
		}
	} else {
		display_activity("WIFI", "CONFIG ERROR", "NO WIFI CONFIG");
		gpio_put(WIFI_LED_PIN, 0);
		return -3;
	}

	return 0;
}
