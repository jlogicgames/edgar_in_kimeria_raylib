.PHONY: macos web build-macos build-web

build-macos:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build

macos:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ./build/edgard_in_kimeria

build-web:
	emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release && cmake --build build-web

web:
	emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release && cmake --build build-web && python3 -m http.server --directory build-web 8000
