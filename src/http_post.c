#include "http_post.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/tls_credentials.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

LOG_MODULE_REGISTER(http_post, LOG_LEVEL_INF);

#define MJOLNIR_SEC_TAG 42

/* Wygenerowane przez generate_inc_file_for_target z src/certs/ca_bundle.pem.
 * generate_inc_file_for_target nie dokleja terminatora \0 — mbedtls parser PEM
 * wymaga null-terminowanego bufora, wiec trailing 0 dodajemy recznie. */
static const unsigned char ca_bundle[] = {
#include "ca_bundle.pem.inc"
    , 0x00
};

static uint8_t recv_buf[1024];
static bool tls_ready;

static void http_response_cb(struct http_response *rsp,
							 enum http_final_call final_data, void *user_data)
{
	ARG_UNUSED(user_data);
	if (final_data == HTTP_DATA_FINAL)
	{
		LOG_INF("HTTP response: status %d, body_len=%zu",
				rsp->http_status_code, rsp->data_len);
		if (rsp->body_frag_start != NULL && rsp->body_frag_len > 0)
		{
			size_t n = rsp->body_frag_len < 256 ? rsp->body_frag_len : 256;
			LOG_INF("body: %.*s", (int)n, rsp->body_frag_start);
		}
	}
}

int http_post_init_tls(void)
{
	if (tls_ready)
	{
		return 0;
	}

	int ret = tls_credential_add(MJOLNIR_SEC_TAG,
								 TLS_CREDENTIAL_CA_CERTIFICATE,
								 ca_bundle, sizeof(ca_bundle));
	if (ret == -EEXIST)
	{
		LOG_WRN("TLS cert sec tag %d already present", MJOLNIR_SEC_TAG);
	}
	else if (ret < 0)
	{
		LOG_ERR("tls_credential_add failed: %d", ret);
		return ret;
	}

	LOG_INF("TLS CA bundle registered under sec_tag %d (%zu bytes)",
			MJOLNIR_SEC_TAG, sizeof(ca_bundle));
	tls_ready = true;
	return 0;
}

static int resolve_host(const char *host, uint16_t port, struct sockaddr_in *out)
{
	struct zsock_addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};
	struct zsock_addrinfo *res = NULL;

	int ret = zsock_getaddrinfo(host, NULL, &hints, &res);
	if (ret != 0 || res == NULL)
	{
		LOG_ERR("DNS resolve %s failed: %d", host, ret);
		return -EHOSTUNREACH;
	}

	memset(out, 0, sizeof(*out));
	out->sin_family = AF_INET;
	out->sin_port = htons(port);
	out->sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
	zsock_freeaddrinfo(res);
	return 0;
}

int http_post_telemetry(const struct telemetry *t)
{
	if (!tls_ready)
	{
		int r = http_post_init_tls();
		if (r < 0)
		{
			return r;
		}
	}

	int sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TLS_1_2);
	if (sock < 0)
	{
		LOG_ERR("socket(TLS) failed: %d", errno);
		return -errno;
	}

	const sec_tag_t sec_tag_list[] = {MJOLNIR_SEC_TAG};
	int verify = TLS_PEER_VERIFY_REQUIRED;

	if (zsock_setsockopt(sock, SOL_TLS, TLS_SEC_TAG_LIST,
						 sec_tag_list, sizeof(sec_tag_list)) < 0)
	{
		LOG_ERR("setsockopt TLS_SEC_TAG_LIST failed: %d", errno);
		zsock_close(sock);
		return -errno;
	}
	if (zsock_setsockopt(sock, SOL_TLS, TLS_HOSTNAME,
						 CONFIG_APP_SERVER_HOST,
						 strlen(CONFIG_APP_SERVER_HOST)) < 0)
	{
		LOG_ERR("setsockopt TLS_HOSTNAME failed: %d", errno);
		zsock_close(sock);
		return -errno;
	}
	if (zsock_setsockopt(sock, SOL_TLS, TLS_PEER_VERIFY,
						 &verify, sizeof(verify)) < 0)
	{
		LOG_ERR("setsockopt TLS_PEER_VERIFY failed: %d", errno);
		zsock_close(sock);
		return -errno;
	}

	struct sockaddr_in addr;
	if (resolve_host(CONFIG_APP_SERVER_HOST,
					 CONFIG_APP_SERVER_PORT, &addr) < 0)
	{
		zsock_close(sock);
		return -EHOSTUNREACH;
	}

	if (zsock_connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		LOG_ERR("connect() to %s:%d failed: %d",
				CONFIG_APP_SERVER_HOST, CONFIG_APP_SERVER_PORT, errno);
		zsock_close(sock);
		return -errno;
	}

	char body[256];
	int body_len = telemetry_build_json(t, body, sizeof(body));
	if (body_len < 0 || body_len >= (int)sizeof(body))
	{
		LOG_ERR("telemetry_build_json: buffer too small (ret=%d)", body_len);
		zsock_close(sock);
		return -ENOMEM;
	}

	const char *headers[] = {
		"X-Mjolnir-Key: " CONFIG_APP_MJOLNIR_API_KEY "\r\n",
		"Content-Type: application/json\r\n",
		"Accept: application/json\r\n",
		NULL,
	};

	struct http_request req = {
		.method = HTTP_POST,
		.url = CONFIG_APP_SERVER_URL_PATH,
		.host = CONFIG_APP_SERVER_HOST,
		.protocol = "HTTP/1.1",
		.payload = body,
		.payload_len = body_len,
		.header_fields = headers,
		.response = http_response_cb,
		.recv_buf = recv_buf,
		.recv_buf_len = sizeof(recv_buf),
	};

	int ret = http_client_req(sock, &req, 15000, NULL);
	zsock_close(sock);

	if (ret < 0)
	{
		LOG_ERR("http_client_req failed: %d", ret);
	}
	return ret;
}
