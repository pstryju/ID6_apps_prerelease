CC ?= cc
CXX ?= c++
PYTHON ?= python3
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Werror
CORE = src/gauge_core.c
HEADERS = src/gauge_core.h src/drive_frame.h src/diagnostic_lease.h
MOST = src/most/dsi_bind.c src/most/dsi_client.c src/most/most_link.c src/most/most_proto.c
MOST_HEADERS = $(wildcard src/most/*.h)
MOST_FAKE = tests/fake_dsi.c tests/fake_dme.c
.PHONY: assets test

assets:
	$(PYTHON) tools/make_bars.py
	$(PYTHON) tools/make_drive_apps.py

build:
	mkdir -p build

build/boostdeck-collector: src/collector.c $(CORE) $(HEADERS) | build
	$(CC) $(CFLAGS) -Isrc $(filter %.c,$^) -lm -o $@

build/shift-deck: src/collector.c $(CORE) $(HEADERS) | build
	$(CC) $(CFLAGS) -Isrc -DSHIFT_DECK_ONLY $(filter %.c,$^) -lm -o $@

build/mhd-maps: src/collector.c $(CORE) $(HEADERS) | build
	$(CC) $(CFLAGS) -Isrc -DMHD_MAPS_ONLY $(filter %.c,$^) -lm -o $@

build/engine-codes: src/collector.c $(CORE) $(HEADERS) | build
	$(CC) $(CFLAGS) -Isrc -DENGINE_CODES_ONLY $(filter %.c,$^) -lm -o $@

build/xdrive-control: src/collector.c src/controls.c $(CORE) $(HEADERS) | build
	$(CC) $(CFLAGS) -Isrc -DXDRIVE_ONLY src/collector.c $(CORE) -lm -o $@

build/test-hud-core: tests/test_hud_core.cpp vendor/hud/hud_core.cpp $(wildcard vendor/hud/cluster/*.hpp vendor/hud/most/*.hpp) src/hud_feed.h | build
	$(CXX) -O2 -std=c++98 -Wall -Wextra -Werror -Isrc -Ivendor/hud tests/test_hud_core.cpp vendor/hud/hud_core.cpp -pthread -o $@

build/diagnostic-probe: src/collector.c $(CORE) $(HEADERS) | build
	$(CC) $(CFLAGS) -Isrc -DDIAGNOSTIC_PROBE_ONLY $(filter %.c,$^) -lm -o $@

build/stock-collector: src/collector.c $(CORE) $(HEADERS) | build
	$(CC) $(CFLAGS) -Isrc -DPERF_LAB -DBD_STOCK_DME $(filter %.c,$^) -lm -o $@

build/test-stock-core: tests/test_stock_core.c $(CORE) $(HEADERS) | build
	$(CC) $(CFLAGS) -Isrc -DPERF_LAB -DBD_STOCK_DME $(filter %.c,$^) -lm -o $@

build/test-core: tests/test_core.c $(CORE) $(HEADERS) | build
	$(CC) $(CFLAGS) -Isrc $(filter %.c,$^) -lm -o $@

build/test-cds: tests/test_cds.cpp src/cds_values.h src/cds_telemetry.h | build
	$(CXX) -O2 -std=c++98 -Wall -Wextra -Werror -Isrc tests/test_cds.cpp -pthread -o $@

build/test-most: tests/test_most.c $(MOST_FAKE) $(MOST) $(MOST_HEADERS) | build
	$(CC) $(CFLAGS) -DMOST_HOST -Isrc -Itests $(filter %.c,$^) -pthread -o $@

build/test-boost-most: src/boost_most.c $(CORE) tests/boost_most_env.c $(MOST_FAKE) $(MOST) $(MOST_HEADERS) | build
	$(CC) $(CFLAGS) -DMOST_HOST -DPERF_LAB -DBD_STOCK_DME -Isrc -Itests $(filter %.c,$^) -pthread -lm -o $@

build/test-hud-boost: src/boost_most.c $(CORE) tests/boost_most_env.c $(MOST_FAKE) $(MOST) $(MOST_HEADERS) | build
	$(CC) $(CFLAGS) -DMOST_HOST -DPERF_LAB -DBD_STOCK_DME -DBD_HUD_SOURCE -Isrc -Itests $(filter %.c,$^) -pthread -lm -o $@

test: build/boostdeck-collector build/shift-deck build/mhd-maps build/engine-codes build/diagnostic-probe build/test-core build/stock-collector build/test-stock-core build/test-cds build/test-most build/test-boost-most build/xdrive-control build/test-hud-core build/test-hud-boost
	./build/test-hud-core
	$(PYTHON) -B tests/test_xdrive.py
	./build/test-most
	$(PYTHON) -B tests/test_boost_most.py
	./build/test-cds
	./build/test-core
	./build/test-stock-core
	$(PYTHON) -B tests/test_transport.py
	$(PYTHON) -B tests/test_drive_apps.py
	$(PYTHON) -B tests/test_stock_transport.py
