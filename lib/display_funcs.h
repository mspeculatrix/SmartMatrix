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
extern AutoFeed autofeed_cfg;
extern char ip_buf[20];
extern const char* error_msg[];
extern char wifi_ssid[];
extern bool wifi_connected;

void init_display();
void display_activity(const char* header, const char* activity,
	const char* status);
void display_AF(void);
void display_SSID(void);
void display_IP(void);
void display_status(SystemStatus state, uint32_t data);


#endif
