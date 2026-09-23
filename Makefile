.PHONY: macos web ios-simulator build-macos build-web build-ios-simulator

build-macos:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build

macos:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ./build/edgard_in_kimeria

build-web:
	emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release && cmake --build build-web

web:
	emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release && cmake --build build-web && python3 -m http.server --directory build-web 8000

build-ios-simulator:
	cmake -S . -B build-ios-simulator -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_BUILD_TYPE=Debug && cmake --build build-ios-simulator --config Debug --parallel 2

ios-simulator: build-ios-simulator
	xcrun simctl bootstatus booted -b && xcrun simctl install booted build-ios-simulator/Debug-iphonesimulator/edgard_in_kimeria.app && xcrun simctl launch booted com.jlogicgames.edgard-in-kimeria
