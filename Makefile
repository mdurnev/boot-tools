all:
	$(MAKE) -C common
	$(MAKE) -C systemd-lttng
	$(MAKE) -C systemd-perf

clean:
	$(MAKE) -C systemd-lttng clean
	$(MAKE) -C systemd-perf clean
	$(MAKE) -C common clean

.PHONY: clean
