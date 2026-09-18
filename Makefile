.PHONY: assets test build package
assets:
	python3 tools/generate_assets.py
	python3 tools/render_preview.py
test: assets
	python3 tests/source_checks.py
	bash tests/host_syntax.sh
build:
	./build.sh
package: test
	python3 tools/package_source.py
