# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

REPO_DIR := ../../../
BENCH_DIR := tests/pytest/benchmark


default: main

.PHONY: setup
setup:
	rm -rf _working
	mkdir _working

.PHONY: main
main: setup
	./benchmark.py run

simer:
	./benchmark.py run --simerexec True

.PHONY: benchmark
benchmark:
	@cd $(REPO_DIR); \
	docker network create dse; \
	docker-compose \
		-f $(BENCH_DIR)/docker-compose.yaml up \
		--exit-code-from benchmark \
		--abort-on-container-exit \
		; \
	docker network inspect dse >/dev/null 2>&1 && \
	docker network inspect -f '{{ range $$key, $$value := .Containers }}{{ printf "%s\n" $$key}}{{ end }}' dse | xargs -r docker kill;docker container rm redis; \
	docker network rm dse > /dev/null 2>&1;

.PHONY: clean
clean:
	rm -rf _out
	rm -rf _working
