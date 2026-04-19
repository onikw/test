#include "wifi_sta.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <string.h>
#include <errno.h>

LOG_MODULE_REGISTER(wifi_sta, LOG_LEVEL_INF);

#define WIFI_MGMT_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT | \
			  NET_EVENT_WIFI_DISCONNECT_RESULT)
#define IPV4_MGMT_EVENTS (NET_EVENT_IPV4_DHCP_BOUND)

static K_SEM_DEFINE(wifi_connected_sem, 0, 1);
static K_SEM_DEFINE(ipv4_bound_sem, 0, 1);

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;

static void wifi_mgmt_handler(struct net_mgmt_event_callback *cb,
			      uint64_t mgmt_event, struct net_if *iface)
{
	const struct wifi_status *status = (const struct wifi_status *)cb->info;

	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		if (status->status) {
			LOG_ERR("WiFi connect failed (%d)", status->status);
		} else {
			LOG_INF("WiFi connected");
			k_sem_give(&wifi_connected_sem);
		}
		break;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		LOG_WRN("WiFi disconnected");
		break;
	default:
		break;
	}
}

static void ipv4_mgmt_handler(struct net_mgmt_event_callback *cb,
			      uint64_t mgmt_event, struct net_if *iface)
{
	if (mgmt_event != NET_EVENT_IPV4_DHCP_BOUND) {
		return;
	}

	const struct net_if_dhcpv4 *dhcp = cb->info;
	char ip_str[NET_IPV4_ADDR_LEN];

	net_addr_ntop(AF_INET, &dhcp->requested_ip, ip_str, sizeof(ip_str));
	LOG_INF("DHCP IP: %s", ip_str);
	k_sem_give(&ipv4_bound_sem);
}

static int wifi_connect_once(void)
{
	struct net_if *iface = net_if_get_first_wifi();
	if (!iface) {
		LOG_ERR("No WiFi iface");
		return -ENODEV;
	}

	struct wifi_connect_req_params params = {
		.ssid = (const uint8_t *)CONFIG_APP_WIFI_SSID,
		.ssid_length = strlen(CONFIG_APP_WIFI_SSID),
		.psk = (const uint8_t *)CONFIG_APP_WIFI_PSK,
		.psk_length = strlen(CONFIG_APP_WIFI_PSK),
		.security = WIFI_SECURITY_TYPE_PSK,
		.channel = WIFI_CHANNEL_ANY,
		.band = WIFI_FREQ_BAND_UNKNOWN,
		.mfp = WIFI_MFP_OPTIONAL,
	};

	LOG_INF("Connecting to SSID: %s", CONFIG_APP_WIFI_SSID);

	int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
	if (ret) {
		LOG_ERR("WiFi connect request failed: %d", ret);
	}
	return ret;
}

int wifi_sta_start(void)
{
	net_mgmt_init_event_callback(&wifi_cb, wifi_mgmt_handler, WIFI_MGMT_EVENTS);
	net_mgmt_add_event_callback(&wifi_cb);

	net_mgmt_init_event_callback(&ipv4_cb, ipv4_mgmt_handler, IPV4_MGMT_EVENTS);
	net_mgmt_add_event_callback(&ipv4_cb);

	while (wifi_connect_once() != 0) {
		k_sleep(K_SECONDS(3));
	}
	return 0;
}

int wifi_sta_wait_ready(int timeout_sec)
{
	if (k_sem_take(&wifi_connected_sem, K_SECONDS(timeout_sec)) != 0) {
		LOG_ERR("WiFi connect timeout");
		return -ETIMEDOUT;
	}
	if (k_sem_take(&ipv4_bound_sem, K_SECONDS(timeout_sec)) != 0) {
		LOG_ERR("DHCP timeout");
		return -ETIMEDOUT;
	}
	return 0;
}
