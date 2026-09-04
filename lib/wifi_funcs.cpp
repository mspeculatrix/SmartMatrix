#include "wifi_funcs.h"

/**
 * @brief Reads stored Wi-Fi credentials from the top Flash sector.
 * @return true if valid credentials exist; false otherwise.
 */
bool load_wifi_credentials(NetworkContext* net) {
	const uint8_t* flash_contents = (const uint8_t*)(XIP_BASE + FL_CFG_OFFSET);
	const WifiCredentials* config = (const WifiCredentials*)flash_contents;

	// Must match magic header AND contain a non-empty SSID
	if (config->magic == WIFI_MAGIC_HEADER && strlen(config->ssid) > 0) {
		strncpy(net->wifi_ssid, config->ssid, WIFI_SSID_LEN + 1);
		strncpy(net->wifi_passwd, config->password, WIFI_PASS_LEN + 1);
		net->wifi_ssid[WIFI_SSID_LEN] = '\0';
		net->wifi_passwd[WIFI_PASS_LEN] = '\0';
		return true;
	}

	return false;
}

/**
 * @brief Erases flash sector and writes new wifi credentials.
 */
void save_wifi_credentials(NetworkContext* net) {
	WifiCredentials config;
	memset(&config, 0, sizeof(config));

	config.magic = WIFI_MAGIC_HEADER;
	strncpy(config.ssid, net->wifi_ssid, WIFI_SSID_LEN);
	strncpy(config.password, net->wifi_passwd, WIFI_PASS_LEN);

	// Prepare buffer aligned to full page size (256 bytes)
	uint8_t buffer[FLASH_PAGE_SIZE];
	memset(buffer, 0xFF, sizeof(buffer)); // Default unprogrammed flash state
	memcpy(buffer, &config, sizeof(config));

	printf(": Saving Wi-Fi credentials to Flash...\n");

	// Disable interrupts to prevent flash execution crash while rewriting
	uint32_t ints = save_and_disable_interrupts();

	flash_range_erase(FL_CFG_OFFSET, FLASH_SECTOR_SIZE);
	flash_range_program(FL_CFG_OFFSET, buffer, FLASH_PAGE_SIZE);

	restore_interrupts(ints);

	printf(": Wi-Fi credentials saved successfully.\n");
}

/**
 * @brief Connect to wifi using configured credentials
 * @param ssid Wifi SSID string
 * @param passwd Wifi password string
 * @param iface pointer to netif interface instance
 * @return int Error code
 */
int wifi_connect(NetworkContext* net) {
	printf(": WIFI connecting to %s\n", net->wifi_ssid);

	int err = -1;
	int retries = 0;
	bool wifi_configured = load_wifi_credentials(net);
	if (wifi_configured) {
		while (err != 0 && retries < WIFI_MAX_TRIES) {
			retries++;
			char bootmsg[24];
			snprintf(bootmsg, sizeof(bootmsg), "CONNECTING (%d/5)...", retries);
			display_activity("WIFI", bootmsg, "awaiting connection");
			if (retries > 1) {
				printf(": WIFI retry attempt %d/%d\n", retries, WIFI_MAX_TRIES);
				gpio_put(WIFI_LED_PIN, 0);
				sleep_ms(2000);
			}
			gpio_put(WIFI_LED_PIN, 1);

			err = cyw43_arch_wifi_connect_timeout_ms(
				net->wifi_ssid,
				net->wifi_passwd,
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

		snprintf(net->ip_buf, sizeof(net->ip_buf), "%s",
			ip4addr_ntoa(netif_ip4_addr(net->netif)));
		printf(": WIFI connected\n");
		printf(": IP address: %s\n", net->ip_buf);

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
