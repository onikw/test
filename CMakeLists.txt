cmake_minimum_required(VERSION 3.20.0)
list(APPEND ZEPHYR_EXTRA_MODULES
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/edge-impulse-sdk-zephyr
    ${CMAKE_CURRENT_SOURCE_DIR}/edge_impulse
)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(test)

target_sources(app PRIVATE
	src/main.c
	src/led.c
	src/led_strip.c
	src/ble_led.c
	src/battery.c
	src/motion.c
	src/activity_indicator.c
	src/telemetry.c
	src/wifi_sta.c
	src/http_post.c
	src/ei_wrapper.cpp
)

zephyr_include_directories(src)

# Certyfikaty CA (bundle: Google Trust Services Root R4 + ISRG Root X1) —
# zagniezdzone do firmware jako tablica bajtow, rejestrowane w tls_credential
# przed pierwszym POST-em HTTPS.
generate_inc_file_for_target(app
    src/certs/ca_bundle.pem
    ${ZEPHYR_BINARY_DIR}/include/generated/ca_bundle.pem.inc)

# Edge Impulse SDK: kod trzeciej strony, nie chcemy żeby jego ostrzeżenia
# (double-promotion, maybe-uninitialized itd.) wywalały nasz build.
if(TARGET edge-impulse-sdk)
    target_compile_options(edge-impulse-sdk PRIVATE -w)
endif()
