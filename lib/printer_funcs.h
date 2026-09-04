/**
 * @file printer_funcs.h
 * @brief Functions for talking to the printer
 */

#ifndef __PRINTER_FUNCS_H__
#define __PRINTER_FUNCS_H__

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "defines.h"
#include "display_funcs.h"

ErrorState check_for_error(void);
void poll_printer_status(SystemContext* sys, NetworkContext* net);
void send_byte_to_printer(uint8_t character);

#endif
