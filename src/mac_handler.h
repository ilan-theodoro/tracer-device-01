#ifndef MAC_HANDLER_H
#define MAC_HANDLER_H

#include "driver/gpio.h"
#include "esp32-hal-uart.h"

uart_t* _uart;

#define MAC_BEGIN_FLAG 128
#define RX_UART 16
#define TX_UART 17

// DATA: FLAG, MAC_FIELD_01, ...02, ...03, ...04, ...05, ...06, RSSI = 8 BYTES
void send_mac_by_uart(const uint8_t mac[6], int rssi) {
		uartWrite(_uart, MAC_BEGIN_FLAG);
		uartWriteBuf(_uart, mac, 6);
		uartWrite(_uart, rssi);
    printf("%02x\n", mac[0]);
}

// INIT COMMUNICATION BETWEEN THE ESP32'S
void uart_init() {
  _uart = uartBegin(1, 9600, SERIAL_8N1, RX_UART, TX_UART, 256, false);
}

#endif // MAC_HANDLER_H
