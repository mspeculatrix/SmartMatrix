#ifndef __DISPLAY_FUNCS_H__
#define __DISPLAY_FUNCS_H__

#include <stdio.h>
#include <string.h>
#include "defines.h"
#include "pico_ssd1306_basic.h"

void init_display();
void display_activity(const char* header, const char* activity,
	const char* status);
void display_status(uint8_t state, uint32_t data);


#endif
