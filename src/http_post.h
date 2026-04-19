#ifndef HTTP_POST_H_
#define HTTP_POST_H_

#include "telemetry.h"

/*
 * Buduje JSON-a z `t` i wysyła jako POST /data na CONFIG_APP_SERVER_HOST.
 * Zwraca 0 przy sukcesie, wartość < 0 przy błędzie socket/HTTP.
 */
int http_post_telemetry(const struct telemetry *t);

#endif /* HTTP_POST_H_ */
