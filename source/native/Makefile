CC ?= cc
CXX ?= c++
PYTHON ?= python3
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Werror
CORE = src/gauge_core.c
CORE_LINK = $(CORE) src/diag_link.c
HEADERS = src/gauge_core.h src/boost_signals.def src/evo_y_gauge_core.h src/evo_y_gauge_core.inc src/evo_y_collector.inc src/drive_frame.h src/diagnostic_lease.h src/diag_link.h src/signal_decode.h $(wildcard src/profiles/*.inc)
# Every host worker carries the HSFZ-then-MOST link, with a fake DSI selected by ID6_FAKE_MOST.
MOST = src/most/most_link.c src/most/dsi_client.c src/most/most_proto.c src/most/dsi_bind.c
MOST_HEADERS = src/most/most_link.h src/most/dsi_client.h src/most/most_proto.h src/most/dsi_abi.h
FAKE_DSI = tests/fake_dsi.c tests/fake_dme.c tests/fake_dsi.h tests/fake_dme.h
WORKER = src/collector.c $(CORE_LINK) $(MOST) tests/fake_most_env.c $(FAKE_DSI) $(HEADERS) $(MOST_HEADERS)
HOST_LINK = $(CC) $(CFLAGS) -pthread -DMOST_HOST -Isrc -Itests
MOST_FAKE = tests/fake_dsi.c tests/fake_dme.c
.PHONY: assets launch-assets test

assets:
	$(PYTHON) tools/make_bars.py
	$(PYTHON) tools/make_drive_apps.py

build:
	mkdir -p build

build/boostdeck-collector: $(WORKER) | build
	$(HOST_LINK) $(filter %.c,$^) -lm -o $@

build/shift-deck: $(WORKER) | build
	$(HOST_LINK) -DSHIFT_DECK_ONLY $(filter %.c,$^) -lm -o $@

build/mhd-maps: $(WORKER) | build
	$(HOST_LINK) -DMHD_MAPS_ONLY $(filter %.c,$^) -lm -o $@

build/engine-codes: $(WORKER) | build
	$(HOST_LINK) -DENGINE_CODES_ONLY $(filter %.c,$^) -lm -o $@

build/xdrive-control: $(WORKER) src/controls.c | build
	$(HOST_LINK) -DXDRIVE_ONLY $(filter %.c,$(filter-out src/controls.c,$^)) -lm -o $@

build/test-hud-trace: tests/test_hud_trace.cpp src/hud_trace.h | build
	$(CXX) -O2 -std=c++98 -Wall -Wextra -Werror -Isrc tests/test_hud_trace.cpp -o $@

build/test-hud-core: tests/test_hud_core.cpp vendor/hud/hud_core.cpp $(wildcard vendor/hud/cluster/*.hpp vendor/hud/most/*.hpp) src/hud_feed.h src/hud_lifecycle.h src/hud_rpm_graph.h | build
	$(CXX) -O2 -std=c++98 -Wall -Wextra -Werror -Isrc -Ivendor/hud tests/test_hud_core.cpp vendor/hud/hud_core.cpp -pthread -o $@

build/diagnostic-probe: $(WORKER) src/controls.c | build
	$(HOST_LINK) -DDIAGNOSTIC_PROBE_ONLY $(filter %.c,$(filter-out src/controls.c,$^)) -lm -o $@

build/stock-collector: $(WORKER) src/controls.c | build
	$(HOST_LINK) -DPERF_LAB -DBD_STOCK_DME $(filter %.c,$(filter-out src/controls.c,$^)) -lm -o $@

build/test-stock-core: tests/test_stock_core.c $(CORE) $(HEADERS) | build
	$(CC) $(CFLAGS) -Isrc -DPERF_LAB -DBD_STOCK_DME $(filter %.c,$^) -lm -o $@

build/test-core: tests/test_core.c $(CORE) $(HEADERS) | build
	$(CC) $(CFLAGS) -Isrc $(filter %.c,$^) -lm -o $@

build/test-diag-link: tests/test_diag_link.c $(CORE_LINK) $(MOST) tests/fake_most_env.c $(FAKE_DSI) $(HEADERS) $(MOST_HEADERS) | build
	$(HOST_LINK) $(filter %.c,$^) -lm -o $@

build/test-cds: tests/test_cds.cpp src/cds_values.h src/cds_telemetry.h | build
	$(CXX) -O2 -std=c++98 -Wall -Wextra -Werror -Isrc tests/test_cds.cpp -pthread -o $@

build/test-most: tests/test_most.c $(MOST_FAKE) $(MOST) $(MOST_HEADERS) | build
	$(CC) $(CFLAGS) -DMOST_HOST -Isrc -Itests $(filter %.c,$^) -pthread -o $@

build/test-engine-profiles: tests/test_engine_profiles.c src/engine_profiles.h $(HEADERS) | build
	$(CC) $(CFLAGS) -DPERF_LAB -DBD_STOCK_DME -Isrc $(filter %.c,$^) -lm -o $@

build/test-boost-most: src/boost_most.c src/engine_profiles.h $(HEADERS) $(CORE) tests/boost_most_env.c $(MOST_FAKE) $(MOST) $(MOST_HEADERS) | build
	$(CC) $(CFLAGS) -DMOST_HOST -DPERF_LAB -DBD_STOCK_DME -Isrc -Itests $(filter %.c,$^) -pthread -lm -o $@

build/test-hud-boost: src/boost_most.c src/engine_profiles.h $(HEADERS) $(CORE) tests/boost_most_env.c $(MOST_FAKE) $(MOST) $(MOST_HEADERS) | build
	$(CC) $(CFLAGS) -DMOST_HOST -DPERF_LAB -DBD_STOCK_DME -DBD_HUD_SOURCE -Isrc -Itests $(filter %.c,$^) -pthread -lm -o $@

build/bootmod3: $(WORKER) src/controls.c src/bm3_profiles.h | build
	$(HOST_LINK) -DBM3_ONLY $(filter-out src/controls.c,$(filter %.c,$^)) -lm -o $@

build/exhaust-flap: $(WORKER) | build
	$(HOST_LINK) -DEXHAUST_FLAP_ONLY $(filter %.c,$^) -lm -o $@

build/test-launch-timer: tests/test_launch_timer.c src/launch_timer.c src/launch_timer.h | build
	$(CC) $(CFLAGS) -Isrc $(filter %.c,$^) -o $@

build/launch_timer.o: src/launch_timer.c src/launch_timer.h | build
	$(CC) $(CFLAGS) -Isrc -c $< -o $@

build/test-launch-records: tests/test_launch_records.cpp build/launch_timer.o src/launch_records.h | build
	$(CXX) -O2 -std=gnu++98 -Wall -Wextra -Werror -Isrc $(filter %.cpp %.o,$^) -o $@

launch-assets:
	$(PYTHON) tools/launch_art.py

test: build/boostdeck-collector build/shift-deck build/mhd-maps build/engine-codes build/diagnostic-probe build/test-core build/stock-collector build/test-stock-core build/test-cds build/test-most build/test-boost-most build/xdrive-control build/test-hud-core build/test-hud-boost build/test-diag-link build/test-engine-profiles build/test-hud-trace build/bootmod3 build/exhaust-flap build/test-launch-timer build/test-launch-records
	./build/test-launch-timer
	./build/test-launch-records
	./build/test-hud-trace
	./build/test-engine-profiles
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
	./build/test-diag-link
	$(PYTHON) -B tests/test_oneshot_most.py
