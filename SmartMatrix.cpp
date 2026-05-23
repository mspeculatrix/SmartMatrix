#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/cyw43_arch.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "lwip/tcp.h"

// ==========================================
// CONFIGURATION & PIN DEFINITIONS
// ==========================================
#define WIFI_SSID "Minerva"
#define WIFI_PASSWORD "Your_Secret_Password"
#define TCP_PORT 9100           // For socket server

// Outputs (via 74HCT541 @ 5V)
#define DATA_MASK       0xFF // GP0 to GP7
#define STROBE_PIN      8
#define INIT_PIN        9
#define AUTOFEED_PIN    10

// Inputs (via 74LVC541 @ 3.3V)
#define BUSY_PIN        11
#define ACK_PIN         12
#define PAPER_END_PIN   13
#define SELECT_PIN      14
#define ERROR_PIN       15

// Future Expansion & Indicators
#define I2C_PORT        i2c0
#define I2C_SDA_PIN     16
#define I2C_SCL_PIN     17
#define LED1_PIN        18
#define LED2_PIN        19

// Command Parser States
enum ParserState {
    STATE_PRINTING,
    STATE_COMMAND
};

char cmd_buffer[256];
size_t cmd_idx = 0;

// ==========================================
// HARDWARE INITIALIZATION
// ==========================================
void init_hardware() {
    // 1. Initialize Parallel Port Data Lines (GP0 - GP7)
    for (int i = 0; i < 8; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_OUT);
    }

    // 2. Initialize Parallel Control Outputs
    const uint32_t outputs[] = { STROBE_PIN, INIT_PIN, AUTOFEED_PIN, LED1_PIN, LED2_PIN };
    for (uint32_t pin : outputs) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
    }

    // Set default idle states for Centronics (Strobe is active low, Init is active low)
    gpio_put(STROBE_PIN, 1);
    gpio_put(INIT_PIN, 1);
    gpio_put(AUTOFEED_PIN, 0);

    // Turn on an LED to indicate boot
    gpio_put(LED1_PIN, 1);
    gpio_put(LED2_PIN, 0);

    // 3. Initialize Parallel Status Inputs
    const uint32_t inputs[] = { BUSY_PIN, ACK_PIN, PAPER_END_PIN, SELECT_PIN, ERROR_PIN };
    for (uint32_t pin : inputs) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        // External buffers drive these, pull-ups/pull-downs are optional but safe
        gpio_disable_pulls(pin);
    }

    // 4. Initialize I2C Port (Configured at 400kHz, but not actively used yet)
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
}

// ==========================================
// CENTRONICS HANDSHAKE PROTOCOL
// ==========================================
void send_byte_to_printer(uint8_t character) {
    // Heartbeat toggle LED2 during active printing
    gpio_put(LED2_PIN, !gpio_get(LED2_PIN));

    // 1. Block while printer is busy
    while (gpio_get(BUSY_PIN)) {
        tight_loop_contents();
    }

    // 2. Put the data byte directly onto GP0-GP7
    gpio_put_masked(DATA_MASK, character);

    // 3. Setup time: allow logic levels to settle
    sleep_us(1);

    // 4. Drop Strobe low to tell printer data is ready
    gpio_put(STROBE_PIN, 0);
    sleep_us(1); // Minimum strobe width is usually 0.5us

    // 5. Pull Strobe back high
    gpio_put(STROBE_PIN, 1);
    sleep_us(1); // Hold time
}

// ==========================================
// COMMAND INTERPRETER
// ==========================================
void handle_command(const char* buffer, size_t length) {
    // Process commands inside 0x01 ... 0x04 blocks
    if (strncmp(buffer, "STATUS", length) == 0) {
        // Read real-time pins via our 3.3V safe LVC buffer
        bool pe = gpio_get(PAPER_END_PIN);
        bool err = !gpio_get(ERROR_PIN); // /ERROR is active low

        if (pe) {
            printf("MSG: ERROR_PAPER_OUT\n");
        } else if (err) {
            printf("MSG: ERROR_PRINTER_FAULT\n");
        } else {
            printf("MSG: PRINTER_READY_OK\n");
        }
    } else if (strncmp(buffer, "RESET", length) == 0) {
        // Pulse the printer's physical /INIT line
        gpio_put(INIT_PIN, 0);
        sleep_ms(100);
        gpio_put(INIT_PIN, 1);
        printf("MSG: PRINTER_RESET_EXECUTED\n");
    } else {
        printf("ERR: UNKNOWN_COMMAND\n");
    }
}

void process_byte(uint8_t byte) {
    static ParserState current_state = STATE_PRINTING;

    if (current_state == STATE_PRINTING) {
        if (byte == 0x01) { // SOH (Start of Header) -> Command Mode
            current_state = STATE_COMMAND;
            cmd_idx = 0;
        } else if (byte >= 0x07) { // Normal printable data block
            send_byte_to_printer(byte);
        }
    } else if (current_state == STATE_COMMAND) {
        if (byte == 0x04) { // EOT (End of Text) -> Process Command
            cmd_buffer[cmd_idx] = '\0';
            handle_command(cmd_buffer, cmd_idx);
            current_state = STATE_PRINTING;
        } else {
            // Protect buffer boundaries
            if (cmd_idx < sizeof(cmd_buffer) - 1) {
                cmd_buffer[cmd_idx++] = (char)byte;
            }
        }
    }
}

// ==========================================
// CORE 1: WI-FI & TCP NETWORKING STACK
// ==========================================
static err_t tcp_recv_callback(void* arg, struct tcp_pcb* tpcb, struct pbuf* p, err_t err) {
    if (p != NULL) {
        tcp_recved(tpcb, p->tot_len);

        // Walk through the raw network buffer chain segment by segment
        for (struct pbuf* q = p; q != NULL; q = q->next) {
            uint8_t* src = (uint8_t*)q->payload;
            for (int i = 0; i < q->len; i++) {
                // Safely stream network packet bytes into the Core Inter-Core FIFO
                multicore_fifo_push_blocking(src[i]);
            }
        }
        pbuf_free(p);
    } else if (err == ERR_OK) {
        // Remote client closed the AppSocket connection safely
        tcp_close(tpcb);
    }
    return ERR_OK;
}

static err_t tcp_accept_callback(void* arg, struct tcp_pcb* newpcb, err_t err) {
    if (err == ERR_OK && newpcb != NULL) {
        tcp_recv(newpcb, tcp_recv_callback);
    }
    return ERR_OK;
}

void core1_entry() {
    if (cyw43_arch_init()) {
        return;
    }

    cyw43_arch_enable_sta_mode();

    // Set network hostname tracking for local DHCP server visibility
    struct netif* netif = &cyw43_state.netif[CYW43_ITF_STA];
    netif_set_hostname(netif, "mx80-smartinterface");

    // Establish link connection to local access point
    if (cyw43_arch_wifi_connect_blocking(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK)) {
        return;
    }

    // Allocate and launch a basic listening RAW TCP server socket on port 9100
    struct tcp_pcb* pcb = tcp_new();
    if (pcb != NULL) {
        if (tcp_bind(pcb, IP_ADDR_ANY, TCP_PORT) == ERR_OK) {
            pcb = tcp_listen(pcb);
            tcp_accept(pcb, tcp_accept_callback);
        }
    }

    // Keep Background Wi-Fi Stack Processing alive indefinitely
    while (true) {
        cyw43_arch_poll();
        sleep_ms(1);
    }
}

// ==========================================
// CORE 0: MAIN PROGRAM & LOCAL SCHEDULER
// ==========================================
int main() {
    // Initialize standard I/O libraries (forces USB CDC Virtual Serial Connection)
    stdio_init_all();
    init_hardware();

    // Fire up the wireless processing layer exclusively on Core 1
    multicore_launch_core1(core1_entry);

    while (true) {
        // 1. Process data streaming from the host PC over local USB-C Virtual Serial Link
        int usb_char = getchar_timeout_us(0);
        if (usb_char != PICO_ERROR_TIMEOUT) {
            process_byte((uint8_t)usb_char);
        }

        // 2. Check if Core 1 (Wi-Fi Stack) has deposited network print stream bytes into the FIFO
        if (multicore_fifo_rvalid()) {
            uint32_t network_char = multicore_fifo_pop_blocking();
            process_byte((uint8_t)network_char);
        }
    }
}
