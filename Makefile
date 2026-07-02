.PHONY: all build test clean wasm

all: build test

build:
	cmake -B build -DCMAKE_BUILD_TYPE=Release
	cmake --build build -j $(shell nproc 2>/dev/null || sysctl -n hw.ncpu)

test: build
	./build/perft_test
	ctest --test-dir build --output-on-failure


wasm:
	npm run build:wasm

clean:
	rm -rf build/

# NOTE: web/public/wasm/ contains committed WASM artifacts required by Vercel.
# Regenerate them intentionally with: npm run build:wasm

