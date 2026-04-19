#ifndef WIFI_STA_H_
#define WIFI_STA_H_

/* Rejestruje callbacki net_mgmt i wysyła request połączenia (retry co 3 s). */
int wifi_sta_start(void);

/* Blokuje aż WiFi się połączy i DHCP przydzieli IP (albo timeout w sekundach). */
int wifi_sta_wait_ready(int timeout_sec);

#endif /* WIFI_STA_H_ */
