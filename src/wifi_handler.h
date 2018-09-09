#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_wifi_types.h"

#include "mac_handler.h"

#define EXAMPLE_ESP_WIFI_MODE_AP   TRUE //TRUE:AP FALSE:STA
#define EXAMPLE_ESP_WIFI_SSID      "ESP32"
#define EXAMPLE_ESP_WIFI_PASS      "password"
#define EXAMPLE_MAX_STA_CONN       4

typedef struct {
	unsigned frame_ctrl:16;
	unsigned duration_id:16;
	uint8_t addr1[6]; /* receiver address */
	uint8_t addr2[6]; /* sender address */
	uint8_t addr3[6]; /* filtering address */
	unsigned sequence_ctrl:16;
	uint8_t addr4[6]; /* optional */
} wifi_ieee80211_mac_hdr_t;

typedef struct {
	wifi_ieee80211_mac_hdr_t hdr;
	uint8_t payload[0]; /* network data ended with 4 bytes csum (CRC32) */
} wifi_ieee80211_packet_t;

#define	WIFI_CHANNEL_MAX		(13)
#define	WIFI_CHANNEL_SWITCH_INTERVAL	(500)

static wifi_country_t wifi_country = {.cc="CN", .schan=1, .nchan=13, .policy=WIFI_COUNTRY_POLICY_AUTO};

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;

static const char *TAG = "TRACER";

//FUNCTION HEADERS

static esp_err_t event_handler(void *ctx, system_event_t *event);
static void wifi_sniffer_init(void);
static void wifi_sniffer_set_channel(uint8_t channel);
static const char *wifi_sniffer_packet_type2str(wifi_promiscuous_pkt_type_t type);
static void wifi_sniffer_packet_handler(void *buff, wifi_promiscuous_pkt_type_t type);

//FUNCTIONS

void wifi_sniffer_init(void) {
	nvs_flash_init();
	tcpip_adapter_init();
	ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
	ESP_ERROR_CHECK( esp_wifi_set_country(&wifi_country) ); /* set country for channel range [1, 13] */
	ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
	wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_AP) );
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
	ESP_ERROR_CHECK( esp_wifi_start() );
	esp_wifi_set_promiscuous(true);
	esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_packet_handler);
}

void wifi_sniffer_set_channel(uint8_t channel) {
	esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

//RETORNA O TIPO DE PACOTE CAPTURADO
const char * wifi_sniffer_packet_type2str(wifi_promiscuous_pkt_type_t type) {
	switch(type) {
		case WIFI_PKT_MGMT: return "MGMT";
		case WIFI_PKT_DATA: return "DATA";
		default:
		case WIFI_PKT_MISC: return "MISC";
	}
}

//FUNCTION TO HANDLE THE RECEIVED PACKETS
void wifi_sniffer_packet_handler(void* buff, wifi_promiscuous_pkt_type_t type) {

	const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buff;
	const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
	const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;

	/*
	//CODE FOR DEBUG PORPOSES
	printf("PACKET TYPE=%s, CHAN=%02d, RSSI=%02d,"
		" ADDR1=%02x:%02x:%02x:%02x:%02x:%02x,"
		" ADDR2=%02x:%02x:%02x:%02x:%02x:%02x,"
		" ADDR3=%02x:%02x:%02x:%02x:%02x:%02x\n",
		wifi_sniffer_packet_type2str(type),
		ppkt->rx_ctrl.channel,
		ppkt->rx_ctrl.rssi,
		 //ADDR1
		hdr->addr1[0],hdr->addr1[1],hdr->addr1[2],
		hdr->addr1[3],hdr->addr1[4],hdr->addr1[5],
		// ADDR2
		hdr->addr2[0],hdr->addr2[1],hdr->addr2[2],
		hdr->addr2[3],hdr->addr2[4],hdr->addr2[5],
		// ADDR3
		hdr->addr3[0],hdr->addr3[1],hdr->addr3[2],
		hdr->addr3[3],hdr->addr3[4],hdr->addr3[5]
	);*/

	int rssi = -ppkt->rx_ctrl.rssi;
	send_mac_by_uart(hdr->addr2, rssi);
	send_mac_by_uart(hdr->addr3, rssi);

}

//CONTROLE DE EVENTOS
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
	    case SYSTEM_EVENT_STA_START:
	        esp_wifi_connect();
	        break;
	    case SYSTEM_EVENT_STA_GOT_IP:
	        ESP_LOGI(TAG, "got ip:%s",
	                 ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
	        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
	        break;
	    case SYSTEM_EVENT_AP_STACONNECTED:
	        ESP_LOGI(TAG, "station:"MACSTR" join, AID=%d",
	                 MAC2STR(event->event_info.sta_connected.mac),
	                 event->event_info.sta_connected.aid);
	        break;
	    case SYSTEM_EVENT_AP_STADISCONNECTED:
	        ESP_LOGI(TAG, "station:"MACSTR"leave, AID=%d",
	                 MAC2STR(event->event_info.sta_disconnected.mac),
	                 event->event_info.sta_disconnected.aid);
	        break;
	    case SYSTEM_EVENT_STA_DISCONNECTED:
	        esp_wifi_connect();
	        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
	        break;
	    default:
	        break;
    }
    return ESP_OK;
}

#endif // WIFI_HANDLER_H
