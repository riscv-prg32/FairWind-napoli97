.PHONY: assets screenshots test build package
assets:
	python3 tools/generate_assets.py
	python3 tools/render_preview.py
screenshots: assets
	python3 tools/host_capture.py --store
test: assets
	python3 tests/source_checks.py
	bash tests/host_syntax.sh
	bash tests/run_harness.sh
build:
	./build.sh
package: test
	python3 tools/package_source.py
