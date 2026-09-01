/**
 * @file serial_funcs.h
 * @brief USB-serial & messaging functions
 */

#ifndef __SERIAL_FUNCS_H__
#define __SERIAL_FUNCS_H__

#include "wifi_funcs.h"
#include "defines.h"

extern SystemStatus system_status;
extern SerialMode serial_mode;
extern char wifi_passwd[WIFI_PASS_LEN + 1];
extern char wifi_ssid[WIFI_SSID_LEN + 1];
extern ErrorState current_err_state;
extern const char* system_status_msg[3];
extern char cmd_buffer[CMD_BUF_LEN];
extern size_t cmd_idx;
extern struct netif* netif;

// Prototype for function in SmartMatrix.cpp
void send_print_byte_to_core1(uint8_t byte);

// Prototypes for this library
void handle_command(const char* buffer, size_t length);
void process_fifo_message(uint32_t msg);
void process_usb_byte(uint8_t byte);

#endif
