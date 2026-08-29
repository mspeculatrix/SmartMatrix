#include "wifi_funcs.h"

extern char ip_buf[IP_BUF_SIZE];

int wifi_connect(char* ssid, char* passwd, struct netif* iface) {
	printf(": WIFI connecting to %s\n", ssid);

	// Wifi association retry loop
	// Resolves AP session state drop delays (ERR_TIMEOUT -8 on soft resets)
	int err = -1;
	int retries = 0;
	while (err != 0 && retries < WIFI_MAX_TRIES) {
		retries++;
		char boot_msg[24];
		snprintf(boot_msg, sizeof(boot_msg), "CONNECTING (%d/5)...", retries);
		display_activity("WIFI", boot_msg, "awaiting connection");
		if (retries > 1) {
			printf(": WIFI retry attempt %d/%d\n", retries, WIFI_MAX_TRIES);
			gpio_put(WIFI_LED_PIN, 0);
			sleep_ms(2000); // Delay to allow AP state machine reset
		}
		gpio_put(WIFI_LED_PIN, 1); // On to indicate attempt to connect

		err = cyw43_arch_wifi_connect_timeout_ms(
			ssid,
			passwd,
			CYW43_AUTH_WPA2_AES_PSK,
			15000 // 15s timeout window per attempt
		);
	}

	if (err != 0) {
		printf("! ERR: FATAL - WIFI CONNECTION FAILED %d\n", err);
		display_activity("WIFI", "NO IP", "WIFI FAILED");
		gpio_put(WIFI_LED_PIN, 0); // Off to show failure to connect
		return -1;
	}

	snprintf(ip_buf, sizeof(ip_buf), "IP: %s", ip4addr_ntoa(netif_ip4_addr(iface)));
	printf(": WIFI connected\n");
	printf(": IP address: %s\n", ip_buf);

	// Flash LED to show success
	for (int i = 0; i < 5; i++) {
		sleep_ms(100);
		gpio_put(WIFI_LED_PIN, 0);
		sleep_ms(100);
		gpio_put(WIFI_LED_PIN, 1);
	}

	return 0;
}
