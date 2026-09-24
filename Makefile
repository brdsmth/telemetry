# Root task runner. Each target fans out to the component Makefiles so one
# command runs everything a laptop can run. See docs/TESTING.md for the tiers.

.PHONY: test test-firmware test-api test-app vectors check-vectors build-firmware

test: check-vectors test-firmware test-api test-app

test-firmware:
	$(MAKE) -C sensor test

test-api:
	$(MAKE) -C api test

test-app:
	cd app && npm test

# Golden vectors are generated and committed; check-vectors fails if the
# committed files are stale.
vectors:
	python3 schema/tools/gen_vectors.py

check-vectors:
	python3 schema/tools/gen_vectors.py --check

build-firmware:
	$(MAKE) -C sensor build
