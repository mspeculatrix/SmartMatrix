/**
 * @file wifi_funcs.h
 * @brief Wifi functions
 */

#ifndef __WIFI_FUNCS_H__
#define __WIFI_FUNCS_H__

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "lwip/tcp.h"
#include "defines.h"
#include "display_funcs.h"

#pragma pack(push, 1)

 // Flash storage configuration
 // Target the last 4KB sector of flash (assuming standard 2MB/4MB flash)
#define FL_CFG_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define WIFI_MAGIC_HEADER   0x57494649 // "WIFI" ASCII identifier

struct WifiCredentials {
	uint32_t magic;
	char ssid[WIFI_SSID_LEN + 1]; 		// Extra byte is for null terminator
	char password[WIFI_PASS_LEN + 1];	//   "    "   "   "   "      "
};

#pragma pack(pop)

int wifi_connect(NetworkContext* net);
bool load_wifi_credentials(NetworkContext* net);
void save_wifi_credentials(NetworkContext* net);

#endif
