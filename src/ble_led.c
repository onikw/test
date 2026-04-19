#include "ble_led.h"

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(ble_led, LOG_LEVEL_INF);

#define LED_SVC_UUID_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)
#define LED_CHAR_UUID_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)

static struct bt_uuid_128 led_svc_uuid = BT_UUID_INIT_128(LED_SVC_UUID_VAL);
static struct bt_uuid_128 led_char_uuid = BT_UUID_INIT_128(LED_CHAR_UUID_VAL);

static bt_addr_le_t target_addr;
static bool target_addr_valid;

static struct bt_conn *current_conn;
static uint16_t led_char_handle;
static struct bt_gatt_discover_params disc_params;

static bool target_configured(void)
{
	return target_addr_valid;
}

static int parse_target_mac(void)
{
	const char *mac_str = CONFIG_APP_BLE_TARGET_MAC;
	const char *type = CONFIG_APP_BLE_TARGET_MAC_TYPE;

	if (mac_str[0] == '\0')
	{
		LOG_WRN("APP_BLE_TARGET_MAC pusty - BLE zostanie wylaczone");
		return -ENODEV;
	}

	int err = bt_addr_le_from_str(mac_str, type, &target_addr);
	if (err)
	{
		LOG_ERR("Zly format MAC '%s' / typu '%s' (err %d)", mac_str, type, err);
		return err;
	}

	char s[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(&target_addr, s, sizeof(s));
	LOG_INF("Target BLE: %s", s);
	target_addr_valid = true;
	return 0;
}

static void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
					struct net_buf_simple *buf);

static int start_scan(void)
{
	if (!target_configured())
	{
		return 0;
	}
	struct bt_le_scan_param sp = {
		.type = BT_LE_SCAN_TYPE_PASSIVE,
		.options = BT_LE_SCAN_OPT_NONE,
		.interval = BT_GAP_SCAN_FAST_INTERVAL,
		.window = BT_GAP_SCAN_FAST_WINDOW,
	};
	int err = bt_le_scan_start(&sp, scan_cb);
	if (err && err != -EALREADY)
	{
		LOG_ERR("scan_start: %d", err);
		return err;
	}
	LOG_INF("Skanuje...");
	return 0;
}

static uint8_t on_char_discovered(struct bt_conn *conn,
								  const struct bt_gatt_attr *attr,
								  struct bt_gatt_discover_params *params)
{
	if (!attr)
	{
		LOG_ERR("Charakterystyka LED nie znaleziona na peryferalu");
		return BT_GATT_ITER_STOP;
	}

	const struct bt_gatt_chrc *chrc = attr->user_data;
	led_char_handle = chrc->value_handle;
	LOG_INF("Charakterystyka LED znaleziona, handle=%u", led_char_handle);
	return BT_GATT_ITER_STOP;
}

static uint8_t on_svc_discovered(struct bt_conn *conn,
								 const struct bt_gatt_attr *attr,
								 struct bt_gatt_discover_params *params)
{
	if (!attr)
	{
		LOG_ERR("Serwis LED nie znaleziony na peryferalu");
		return BT_GATT_ITER_STOP;
	}

	const struct bt_gatt_service_val *svc = attr->user_data;
	LOG_INF("Serwis LED znaleziony, szukam charakterystyki...");

	memset(&disc_params, 0, sizeof(disc_params));
	disc_params.uuid = &led_char_uuid.uuid;
	disc_params.func = on_char_discovered;
	disc_params.start_handle = attr->handle + 1;
	disc_params.end_handle = svc->end_handle;
	disc_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

	int err = bt_gatt_discover(conn, &disc_params);
	if (err)
	{
		LOG_ERR("char discover: %d", err);
	}
	return BT_GATT_ITER_STOP;
}

static int discover_led_service(struct bt_conn *conn)
{
	memset(&disc_params, 0, sizeof(disc_params));
	disc_params.uuid = &led_svc_uuid.uuid;
	disc_params.func = on_svc_discovered;
	disc_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	disc_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
	disc_params.type = BT_GATT_DISCOVER_PRIMARY;
	return bt_gatt_discover(conn, &disc_params);
}

static void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
					struct net_buf_simple *buf)
{
	ARG_UNUSED(rssi);
	ARG_UNUSED(adv_type);
	ARG_UNUSED(buf);

	if (!target_configured())
		return;
	if (current_conn != NULL)
		return;
	if (bt_addr_le_cmp(addr, &target_addr) != 0)
		return;

	int err = bt_le_scan_stop();
	if (err)
	{
		LOG_WRN("scan_stop: %d", err);
	}

	struct bt_conn *conn = NULL;
	err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
							BT_LE_CONN_PARAM_DEFAULT, &conn);
	if (err)
	{
		LOG_ERR("conn_le_create: %d", err);
		(void)start_scan();
		return;
	}
	bt_conn_unref(conn); /* connected callback dostanie wlasna ref */
}

static void on_connected(struct bt_conn *conn, uint8_t err)
{
	char s[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), s, sizeof(s));

	if (err)
	{
		LOG_ERR("Nie udalo sie polaczyc z %s (%u)", s, err);
		(void)start_scan();
		return;
	}

	LOG_INF("Polaczono z %s", s);
	current_conn = bt_conn_ref(conn);
	led_char_handle = 0;

	int derr = discover_led_service(conn);
	if (derr)
	{
		LOG_ERR("discover svc: %d", derr);
		bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	}
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Rozlaczono (0x%02x)", reason);
	if (current_conn)
	{
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}
	led_char_handle = 0;
	(void)start_scan();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = on_connected,
	.disconnected = on_disconnected,
};

bool ble_led_is_ready(void)
{
	return current_conn != NULL && led_char_handle != 0;
}

int ble_led_send(const uint8_t *data, size_t len)
{
	if (!ble_led_is_ready())
	{
		return -ENOTCONN;
	}
	int err = bt_gatt_write_without_response(current_conn, led_char_handle,
											 data, len, false);
	if (err)
	{
		LOG_WRN("write: %d", err);
	}
	return err;
}

int ble_led_start(void)
{
	int err = bt_enable(NULL);
	if (err)
	{
		LOG_ERR("bt_enable: %d", err);
		return err;
	}

	bt_addr_le_t self;
	size_t count = 1;
	bt_id_get(&self, &count);
	char s[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(&self, s, sizeof(s));
	LOG_INF("BLE start. Self MAC: %s", s);

	if (parse_target_mac() != 0)
	{
		return 0; /* BT enabled but nothing to connect to */
	}

	return start_scan();
}
