/**
 * @file display_funcs.h
 * @brief OLED display functions
 */

#ifndef __DISPLAY_FUNCS_H__
#define __DISPLAY_FUNCS_H__

#include <stdio.h>
#include <string.h>
#include "defines.h"
#include "pico_ssd1306_basic.h"

extern ssd1306_t display;

void init_display(void);
void display_activity(const char* header, const char* activity,
	const char* status);
void display_AF(SystemContext* sys);
void display_SSID(NetworkContext* net);
void display_IP(NetworkContext* net);
void display_status(SystemContext* sys, NetworkContext* net, SystemStatus state,
	uint32_t data);


#endif
