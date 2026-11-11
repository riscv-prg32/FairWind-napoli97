.PHONY: assets screenshots preview test build package
assets:
	python3 tools/generate_assets.py
	python3 tools/build_audio.py
screenshots: assets
	python3 tools/host_capture.py --store
preview:
	python3 scripts/qemu_preview.py --script preview --video --seconds 190 --fps 12 --out build/qemu-preview
	python3 scripts/qemu_preview.py --recut --cut 0:21,24:42,62:83 --fps 12 --out build/qemu-preview
	cp build/qemu-preview/preview.mp4 release-artifacts/FairWind-napoli97-preview-60s.mp4
test: assets
	python3 tests/source_checks.py
	bash tests/host_syntax.sh
	bash tests/run_harness.sh
build:
	./build.sh
package: test
	python3 tools/package_source.py
