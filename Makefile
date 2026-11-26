CC ?= cc
AR ?= ar
ARM_CC ?= arm-buildroot-linux-uclibcgnueabi-gcc
ARM_STRIP ?= arm-buildroot-linux-uclibcgnueabi-strip
ARM_SYSROOT_LIB ?= /tmp/zxic-arm-sysroot/lib
SQLITE_HEADER ?= /usr/include/sqlite3.h
CPPFLAGS += -Isrc
CRYPTO_CPPFLAGS = -Ithird_party/mbedtls/include -Ithird_party/mbedtls/library -DMBEDTLS_CONFIG_FILE='"zxct_mbedtls_config.h"'
CRYPTO_SOURCES = third_party/mbedtls/library/md.c third_party/mbedtls/library/md5.c third_party/mbedtls/library/sha1.c third_party/mbedtls/library/platform_util.c third_party/mbedtls/library/base64.c third_party/mbedtls/library/constant_time.c
CFLAGS ?= -O2
CFLAGS += -std=c99 -Wall -Wextra -Werror
DEPFLAGS = -MMD -MP
ARM_FLAGS = -Isrc -std=c99 -Os -march=armv7-a -mfloat-abi=soft -fno-tree-vectorize -ffunction-sections -fdata-sections -Wl,--gc-sections

.PHONY: all test arm-probes arm clean
all: build/zxct-ims

build:
	mkdir -p $@

build/sms_store.o: src/sms_store.c src/sms_store.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/at_http.o: src/at_http.c src/at_http.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/usim.o: src/usim.c src/usim.h src/at_http.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/sms_pdu.o: src/sms_pdu.c src/sms_pdu.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/sms_receipt.o: src/sms_receipt.c src/sms_receipt.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/sms_report.o: src/sms_report.c src/sms_report.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/sms_incoming.o: src/sms_incoming.c src/sms_incoming.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/sip_message.o: src/sip_message.c src/sip_message.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/sip_identity.o: src/sip_identity.c src/sip_identity.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/sip_transaction.o: src/sip_transaction.c src/sip_transaction.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/ims_digest.o: src/ims_digest.c src/ims_digest.h src/zxct_mbedtls_config.h | build
	$(CC) $(CPPFLAGS) $(CRYPTO_CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/ims_challenge.o: src/ims_challenge.c src/ims_challenge.h src/zxct_mbedtls_config.h | build
	$(CC) $(CPPFLAGS) $(CRYPTO_CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/esp_null.o: src/esp_null.c src/esp_null.h src/ims_digest.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/udp6.o: src/udp6.c src/udp6.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/modem_info.o: src/modem_info.c src/modem_info.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/operator_gate.o: src/operator_gate.c src/operator_gate.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/ims_app.o: src/ims_app.c src/ims_app.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/ims_pdn.o: src/ims_pdn.c src/ims_pdn.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/ims_pdn_interface.o: src/ims_pdn_interface.c src/ims_pdn.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/ims_activate.o: src/ims_activate.c src/ims_activate.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/ims_registration.o: src/ims_registration.c src/ims_registration.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/ims_runtime.o: src/ims_runtime.c src/ims_runtime.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/main.o: src/main.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/ims_security.o: src/ims_security.c src/ims_security.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/ims_register.o: src/ims_register.c src/ims_register.h | build
	$(CC) $(CPPFLAGS) $(CRYPTO_CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/ims_transport.o: src/ims_transport.c src/ims_transport.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/ims_socket.o: src/ims_socket.c src/ims_socket.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/ims_identity.o: src/ims_identity.c src/ims_identity.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/mbedtls_%.o: third_party/mbedtls/library/%.c src/zxct_mbedtls_config.h | build
	$(CC) $(CPPFLAGS) $(CRYPTO_CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/libzxct.a: build/sms_store.o build/at_http.o build/usim.o build/sms_pdu.o build/sms_receipt.o build/sms_report.o build/sms_incoming.o build/sip_message.o build/sip_identity.o build/sip_transaction.o build/ims_digest.o build/ims_challenge.o build/ims_security.o build/ims_register.o build/ims_registration.o build/ims_transport.o build/ims_socket.o build/ims_identity.o build/operator_gate.o build/ims_app.o build/ims_pdn.o build/ims_activate.o build/esp_null.o build/udp6.o build/modem_info.o build/mbedtls_md.o build/mbedtls_md5.o build/mbedtls_sha1.o build/mbedtls_platform_util.o build/mbedtls_base64.o build/mbedtls_constant_time.o
	$(AR) rcs $@ $^

build/ui_server.o: src/ui_server.c src/ui_server.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/zxct-ims: build/main.o build/ui_server.o build/ims_runtime.o build/ims_pdn_interface.o build/libzxct.a
	$(CC) $(CFLAGS) $^ -lsqlite3 -ldl -o $@

build/test_sms_store: tests/test_sms_store.c src/sms_store.c src/sms_store.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_sms_store.c src/sms_store.c -lsqlite3 -o $@

build/at_http_probe: tests/at_http_probe.c src/at_http.c src/at_http.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/at_http_probe.c src/at_http.c -o $@

build/test_usim: tests/test_usim.c src/usim.c src/usim.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DUSIM_STATE_PATH='"/tmp/zxct-test-aka.state"' tests/test_usim.c src/usim.c -o $@

build/test_sms_pdu: tests/test_sms_pdu.c src/sms_pdu.c src/sms_pdu.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_sms_pdu.c src/sms_pdu.c -o $@

build/test_sip_identity: tests/test_sip_identity.c src/sip_identity.c src/sip_message.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

build/test_sip_message: tests/test_sip_message.c src/sip_message.c src/sip_message.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_sip_message.c src/sip_message.c -o $@

build/test_ims_digest: tests/test_ims_digest.c src/ims_digest.c $(CRYPTO_SOURCES) src/zxct_mbedtls_config.h | build
	$(CC) $(CPPFLAGS) $(CRYPTO_CPPFLAGS) $(CFLAGS) tests/test_ims_digest.c src/ims_digest.c $(CRYPTO_SOURCES) -o $@

build/test_esp_null: tests/test_esp_null.c src/esp_null.c src/ims_digest.c $(CRYPTO_SOURCES) src/zxct_mbedtls_config.h | build
	$(CC) $(CPPFLAGS) $(CRYPTO_CPPFLAGS) $(CFLAGS) tests/test_esp_null.c src/esp_null.c src/ims_digest.c $(CRYPTO_SOURCES) -o $@

build/test_udp6: tests/test_udp6.c src/udp6.c src/udp6.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_udp6.c src/udp6.c -o $@

build/test_ims_challenge: tests/test_ims_challenge.c src/ims_challenge.c $(CRYPTO_SOURCES) src/zxct_mbedtls_config.h | build
	$(CC) $(CPPFLAGS) $(CRYPTO_CPPFLAGS) $(CFLAGS) tests/test_ims_challenge.c src/ims_challenge.c $(CRYPTO_SOURCES) -o $@

build/test_modem_info: tests/test_modem_info.c src/modem_info.c src/modem_info.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_modem_info.c src/modem_info.c -o $@

build/test_ims_security: tests/test_ims_security.c src/ims_security.c src/ims_security.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_ims_security.c src/ims_security.c -o $@

build/test_ims_register: tests/test_ims_register.c src/ims_register.c src/ims_security.c src/ims_digest.c src/sip_message.c $(CRYPTO_SOURCES) | build
	$(CC) $(CPPFLAGS) $(CRYPTO_CPPFLAGS) $(CFLAGS) $^ -o $@

build/test_ims_transport: tests/test_ims_transport.c src/ims_transport.c src/udp6.c src/esp_null.c src/ims_digest.c $(CRYPTO_SOURCES) | build
	$(CC) $(CPPFLAGS) $(CRYPTO_CPPFLAGS) $(CFLAGS) $^ -o $@

build/test_sip_transaction: tests/test_sip_transaction.c src/sip_transaction.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

build/test_ims_identity: tests/test_ims_identity.c src/ims_identity.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

build/test_sms_receipt: tests/test_sms_receipt.c src/sms_incoming.c src/sms_report.c src/sip_message.c src/sms_receipt.c src/sms_store.c src/sms_pdu.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lsqlite3 -o $@

build/test_sms_report: tests/test_sms_report.c src/sms_report.c src/sip_message.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

build/test_ims_app: tests/test_ims_app.c src/ims_app.c src/operator_gate.c src/modem_info.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

build/test_ims_pdn: tests/test_ims_pdn.c src/ims_pdn.c src/modem_info.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

build/test_ims_activate: tests/test_ims_activate.c src/ims_activate.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -ldl -o $@

build/test_ims_registration: tests/test_ims_registration.c src/ims_registration.c src/ims_register.c src/ims_challenge.c src/ims_security.c src/ims_transport.c src/sip_message.c src/sip_identity.c src/udp6.c src/esp_null.c src/ims_digest.c $(CRYPTO_SOURCES) | build
	$(CC) $(CPPFLAGS) $(CRYPTO_CPPFLAGS) $(CFLAGS) $^ -o $@

test: build/test_sip_identity build/test_sms_store build/test_usim build/test_sms_pdu build/test_sip_message build/test_ims_digest build/test_esp_null build/test_udp6 build/test_ims_challenge build/test_modem_info build/test_ims_security build/test_ims_register build/test_ims_transport build/test_sip_transaction build/test_ims_identity build/test_sms_receipt build/test_sms_report build/test_ims_app build/test_ims_pdn build/test_ims_activate build/test_ims_registration
	./build/test_sip_identity
	./build/test_sms_store
	./build/test_usim
	./build/test_sms_pdu
	./build/test_sip_message
	./build/test_ims_digest
	./build/test_esp_null
	./build/test_udp6
	./build/test_ims_challenge
	./build/test_modem_info
	./build/test_ims_security
	./build/test_ims_register
	./build/test_ims_transport
	./build/test_sip_transaction
	./build/test_ims_identity
	./build/test_sms_receipt
	./build/test_sms_report
	./build/test_ims_app
	./build/test_ims_pdn
	./build/test_ims_activate
	./build/test_ims_registration

build/arm-include/sqlite3.h: $(SQLITE_HEADER) | build
	mkdir -p build/arm-include
	cp $< $@

build/at_http_probe.arm: tests/at_http_probe.c src/at_http.c src/at_http.h | build
	$(ARM_CC) $(ARM_FLAGS) tests/at_http_probe.c src/at_http.c -L$(ARM_SYSROOT_LIB) -Wl,-rpath-link,$(ARM_SYSROOT_LIB) -Wl,--no-as-needed -l:libdl.so.0 -l:libc.so.0 -Wl,--as-needed -o $@
	$(ARM_STRIP) $@

build/test_sms_store.arm: tests/test_sms_store.c src/sms_store.c src/sms_store.h build/arm-include/sqlite3.h
	$(ARM_CC) $(ARM_FLAGS) -Ibuild/arm-include tests/test_sms_store.c src/sms_store.c -L$(ARM_SYSROOT_LIB) -Wl,-rpath-link,$(ARM_SYSROOT_LIB) -Wl,--no-as-needed -l:libpthread.so.0 -l:libsqlite.so -l:libc.so.0 -Wl,--as-needed -o $@
	$(ARM_STRIP) $@

arm-probes: build/at_http_probe.arm build/test_sms_store.arm

ARM_SOURCES = src/main.c src/ui_server.c src/ims_runtime.c src/ims_app.c src/operator_gate.c src/modem_info.c src/ims_pdn.c src/ims_pdn_interface.c src/ims_activate.c src/at_http.c src/usim.c src/ims_identity.c src/sip_message.c src/sip_identity.c src/sip_transaction.c src/ims_register.c src/ims_registration.c src/ims_challenge.c src/ims_security.c src/ims_transport.c src/ims_socket.c src/ims_digest.c src/esp_null.c src/udp6.c src/sms_incoming.c src/sms_report.c src/sms_receipt.c src/sms_pdu.c src/sms_store.c $(CRYPTO_SOURCES)

build/zxct-ims.arm: $(ARM_SOURCES) $(wildcard src/*.h) build/arm-include/sqlite3.h
	$(ARM_CC) $(ARM_FLAGS) $(CRYPTO_CPPFLAGS) -Ibuild/arm-include $(ARM_SOURCES) -L$(ARM_SYSROOT_LIB) -Wl,-rpath-link,$(ARM_SYSROOT_LIB) -Wl,--no-as-needed -l:libpthread.so.0 -l:libsqlite.so -l:libdl.so.0 -l:libc.so.0 -Wl,--as-needed -o $@
	$(ARM_STRIP) $@

build/zxct-ims.arm.gz: build/zxct-ims.arm
	gzip -n -9 -c $< > $@

arm: build/zxct-ims.arm.gz

clean:
	rm -rf build

-include $(wildcard build/*.d)
