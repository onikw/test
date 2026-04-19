#include "http_post.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/http/client.h>
#include <errno.h>

LOG_MODULE_REGISTER(http_post, LOG_LEVEL_INF);

static uint8_t recv_buf[512];

static void http_response_cb(struct http_response *rsp,
			     enum http_final_call final_data, void *user_data)
{
	if (final_data == HTTP_DATA_FINAL) {
		LOG_INF("HTTP response: %s (status %d)",
			rsp->http_status, rsp->http_status_code);
	}
}

int http_post_telemetry(const struct telemetry *t)
{
	int sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		LOG_ERR("socket() failed: %d", errno);
		return -errno;
	}

	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(CONFIG_APP_SERVER_PORT),
	};

	if (zsock_inet_pton(AF_INET, CONFIG_APP_SERVER_HOST, &addr.sin_addr) != 1) {
		LOG_ERR("Bad server IP: %s", CONFIG_APP_SERVER_HOST);
		zsock_close(sock);
		return -EINVAL;
	}

	if (zsock_connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		LOG_ERR("connect() to %s:%d failed: %d",
			CONFIG_APP_SERVER_HOST, CONFIG_APP_SERVER_PORT, errno);
		zsock_close(sock);
		return -errno;
	}

	char body[192];
	int body_len = telemetry_build_json(t, body, sizeof(body));

	const char *headers[] = {
		"Content-Type: application/json\r\n",
		NULL,
	};

	struct http_request req = {
		.method = HTTP_POST,
		.url = "/data",
		.host = CONFIG_APP_SERVER_HOST,
		.protocol = "HTTP/1.1",
		.payload = body,
		.payload_len = body_len,
		.header_fields = headers,
		.response = http_response_cb,
		.recv_buf = recv_buf,
		.recv_buf_len = sizeof(recv_buf),
	};

	int ret = http_client_req(sock, &req, 5000, NULL);
	zsock_close(sock);

	if (ret < 0) {
		LOG_ERR("http_client_req failed: %d", ret);
	}
	return ret;
}
