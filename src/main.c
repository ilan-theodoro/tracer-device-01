#include "mac_handler.h"
#include "wifi_handler.h"

void app_main() {
	uint8_t channel = 1;

	/* setup */
	wifi_sniffer_init();
	uart_init();

	/* loop */
	while (true) {
		vTaskDelay(WIFI_CHANNEL_SWITCH_INTERVAL / portTICK_PERIOD_MS);
		wifi_sniffer_set_channel(channel);
		channel = (channel % WIFI_CHANNEL_MAX) + 1;
	}
}
