.PHONY: macos web ios-simulator build-macos build-web build-ios-simulator release-macos ios-testflight

MACOS_BUILD_DIR ?= build-release-macos
MACOS_APP := $(MACOS_BUILD_DIR)/edgard_in_kimeria.app
MACOS_SIGN_IDENTITY ?=
NOTARY_PROFILE ?=
IOS_BUILD_DIR ?= build-ios-device
IOS_ARCHIVE ?= $(IOS_BUILD_DIR)/EdgardInKimeria.xcarchive
IOS_EXPORT_DIR ?= $(IOS_BUILD_DIR)/export
IOS_DEVELOPMENT_TEAM ?=
IOS_EXPORT_OPTIONS_PLIST ?=
APP_STORE_CONNECT_KEY_ID ?=
APP_STORE_CONNECT_ISSUER_ID ?=

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

# The signing identity and notary profile are deliberately supplied by the release
# machine; neither certificate material nor Apple credentials belong in the repository.
release-macos:
	@test -n "$(MACOS_SIGN_IDENTITY)" || { echo "Set MACOS_SIGN_IDENTITY to a Developer ID Application identity." >&2; exit 2; }
	@test -n "$(NOTARY_PROFILE)" || { echo "Set NOTARY_PROFILE to a configured notarytool keychain profile." >&2; exit 2; }
	cmake -S . -B $(MACOS_BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(MACOS_BUILD_DIR) --config Release --parallel 2
	codesign --force --options runtime --timestamp --sign "$(MACOS_SIGN_IDENTITY)" "$(MACOS_APP)"
	rm -f $(MACOS_BUILD_DIR)/EdgardInKimeria-notarization.zip
	ditto -c -k --keepParent "$(MACOS_APP)" $(MACOS_BUILD_DIR)/EdgardInKimeria-notarization.zip
	xcrun notarytool submit $(MACOS_BUILD_DIR)/EdgardInKimeria-notarization.zip --keychain-profile "$(NOTARY_PROFILE)" --wait
	xcrun stapler staple "$(MACOS_APP)"
	spctl --assess --type execute --verbose=4 "$(MACOS_APP)"

# Produce an App Store Connect upload and submit it to TestFlight. The export-options
# plist selects the intended distribution method and is intentionally local to the
# release machine because it carries account-specific team and provisioning settings.
ios-testflight:
	@test -n "$(IOS_DEVELOPMENT_TEAM)" || { echo "Set IOS_DEVELOPMENT_TEAM to the Apple development team ID." >&2; exit 2; }
	@test -n "$(IOS_EXPORT_OPTIONS_PLIST)" || { echo "Set IOS_EXPORT_OPTIONS_PLIST to an App Store export-options plist." >&2; exit 2; }
	@test -n "$(APP_STORE_CONNECT_KEY_ID)" || { echo "Set APP_STORE_CONNECT_KEY_ID to an App Store Connect API key ID." >&2; exit 2; }
	@test -n "$(APP_STORE_CONNECT_ISSUER_ID)" || { echo "Set APP_STORE_CONNECT_ISSUER_ID to its issuer ID." >&2; exit 2; }
	cmake -S . -B $(IOS_BUILD_DIR) -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_BUILD_TYPE=Release -DEIK_IOS_DEVELOPMENT_TEAM=$(IOS_DEVELOPMENT_TEAM)
	xcodebuild -project $(IOS_BUILD_DIR)/edgard_in_kimeria.xcodeproj -scheme edgard_in_kimeria -configuration Release -archivePath "$(IOS_ARCHIVE)" archive
	rm -rf "$(IOS_EXPORT_DIR)"
	xcodebuild -exportArchive -archivePath "$(IOS_ARCHIVE)" -exportOptionsPlist "$(IOS_EXPORT_OPTIONS_PLIST)" -exportPath "$(IOS_EXPORT_DIR)"
	xcrun altool --upload-app --type ios --file "$(IOS_EXPORT_DIR)/edgard_in_kimeria.ipa" --apiKey "$(APP_STORE_CONNECT_KEY_ID)" --apiIssuer "$(APP_STORE_CONNECT_ISSUER_ID)"
