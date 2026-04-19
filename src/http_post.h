#ifndef HTTP_POST_H_
#define HTTP_POST_H_

#include "telemetry.h"

/*
 * Buduje JSON-a z `t` i wysyla jako POST do Mjolnir Ingest API:
 *   https://CONFIG_APP_SERVER_HOST:CONFIG_APP_SERVER_PORT
 *        CONFIG_APP_SERVER_URL_PATH  (domyslnie /api/mjolnir/ingest)
 * Uzywa HTTPS (TLS 1.2) z walidacja certu po CONFIG_APP_SERVER_HOST.
 * Dokleja header X-Mjolnir-Key z CONFIG_APP_MJOLNIR_API_KEY.
 * Zwraca 0 przy sukcesie, wartosc < 0 przy bledzie socket/DNS/HTTP.
 */
int http_post_telemetry(const struct telemetry *t);

/* Jednorazowo rejestruje certyfikat CA w TLS_CREDENTIALS. */
int http_post_init_tls(void);

#endif /* HTTP_POST_H_ */
