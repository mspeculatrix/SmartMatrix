/**
 * @file serial_funcs.h
 * @brief USB-serial & messaging functions
 */

#ifndef __SERIAL_FUNCS_H__
#define __SERIAL_FUNCS_H__

#include "wifi_funcs.h"
#include "defines.h"

void handle_command(SystemContext* sys, NetworkContext* net);
void process_fifo_message(SystemContext* sys,
	NetworkContext* net,
	uint32_t msg);
void process_usb_byte(SystemContext* sys, NetworkContext* net, uint8_t byte);

#endif
