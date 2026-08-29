#ifndef __WIFI_FUNCS_H__
#define __WIFI_FUNCS_H__

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "defines.h"
#include "display_funcs.h"

int wifi_connect(char* ssid, char* passwd, struct netif* iface);

#endif
