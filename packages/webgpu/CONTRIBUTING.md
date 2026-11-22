# Contributing

## Contributing with coding agents

We happily accept contributions made with coding agents. Our ask is simple: if you send us agent-written content, in a pull request or an issue, start with a few sentences in your own words covering how you came across the problem, whether you experienced it yourself or it came up in an audit, a sense of how much of a pain point it is, and what your own understanding of the issue is. A partial understanding is fine; any attempt at owning the change goes a long way. Agent-written content after that is welcome.

## Development workflow

```sh
yarn
yarn workspace react-native-webgpu copy-dawn-headers
```

### Using pre-built binaries

Dawn, the WebGPU implementation, comes from the shared [React Native Skia binaries release](https://github.com/wcandillon/react-native-skia/releases) pinned by `packages/webgpu/binaries.json`. There is no `postinstall` step:

- On Android, Gradle downloads one Dawn archive per ABI being built, plus the headers. They are resolved as Gradle dependencies, cached in the Gradle user home, and available offline on later builds. The `prepareDawnBinaries` task verifies the pinned sha256 checksums and extracts them into `android/build/binaries`.
- On Apple platforms, an Xcode build phase prepares the selected SDK's Dawn archive from that same pinned release. CocoaPods adds the phase during `pod install`; optional SwiftPM integration adds it during `react-native spm update`. Headers ship in the main `react-native-webgpu` package, without binary npm dependencies. See [Apple binary preparation](#how-apple-binaries-are-prepared) for caching, local builds, and errors.

See [installation](../../apps/docs/content/docs/getting-started/installation.mdx#prebuilt-binaries) for downloads and mirrors on both platforms.

### Examples and tests

Example app (from `apps/example/`): `yarn start` · `yarn ios` · `yarn android`

The [macOS runner](../../apps/example-macos/README.md) requires macOS 14.0 or newer and shares the gallery with an isolated RN macOS 0.83 dependency stack.

From the repo root: `yarn lint` · `yarn tsc` · `yarn build:docs`

Tests (from `packages/webgpu/`): `yarn test:ref` (Chrome reference) · `yarn test` (E2E) · `yarn test:plugin` (Expo config plugin unit tests) · `yarn test:cpp` (C++ unit tests of `cpp/rnwgpu/DawnBlobCache.cpp`, built for the host with CMake and a C++20 compiler against Dawn's headers; one test after a build: `ctest --test-dir node_modules/.cache/cpp-tests -R <name>`)

`yarn test` needs the example app open on its "Tests" screen and connected to Metro first: start Metro with `CI=true yarn start` (from `apps/example/`) — `initialRouteName` in `apps/example/src/App.tsx` is `"Tests"` when `process.env.CI === "true"`, inlined at bundle time via the `transform-inline-environment-variables` babel plugin — then launch the app (`yarn ios` / `yarn android`, or install+launch an existing build). The screen shows "Connecting to localhost..." once it's ready; `yarn test` then picks it up automatically. If a run fails mid-suite, reload the app (`curl -X POST http://localhost:8081/reload`) before the next run — a broken `GPUDevice` from the failed run makes unrelated later tests fail too. The app and the test server talk over port 4242 by default; set `E2E_PORT` for both Metro (it is inlined into the bundle, so start Metro with `--reset-cache` after changing it) and `yarn test` to use another one, as CI does to keep clear of the other projects sharing its runner.

Other `packages/webgpu` scripts: `yarn copy-dawn-headers` runs the cross-platform `scripts/prepare-dawn-headers.mjs` to prepare source/generated release headers in `cpp/dawn/include`; `yarn check-dawn` checks the pinned Dawn commit against the C++ workarounds. CI prepares headers before these checks and before publishing npm.

## Expo config plugin

The Expo config plugin lives in `plugin/src` and is compiled to `plugin/build` by `yarn build:plugin` (also part of `yarn build`, so releases always ship it). `app.plugin.js` at the package root is the entry point Expo resolves when apps list `"react-native-webgpu"` in their `plugins`. The plugin disables Metal API validation in the generated iOS Xcode scheme (required on the iOS Simulator) and raises the Android app's `minSdkVersion` to the library's floor, `androidMinSdkVersion` in `package.json`; it is released together with the package by the regular release workflow, so plugin and package versions are always aligned.

## Upgrading Dawn

Dawn comes from the `binaries-<version>` release built by react-native-skia from its pinned Dawn commit. `packages/webgpu/binaries.json` records the version, commit, and asset checksums for both Android and Apple.

Steps to bump to a new Dawn (new Skia milestone `m<N>`):

1. **Wait for the binaries.** react-native-skia publishes the GitHub release `binaries-<version>`, including the SDK-grouped Apple ZIPs and `dawn-headers.zip`.

2. **Update the pin and headers.** Replace `packages/webgpu/binaries.json` with that release's `checksums.json`, then run `yarn copy-dawn-headers`. Both native integrations read this pin; there is no binary npm dependency to bump.

3. **Review the workarounds.** `yarn check-dawn` fails when the Dawn commit changed while a workaround in `scripts/check-dawn.ts` is still pinned to the previous one: check whether the new Dawn carries the upstream fix, then drop the workaround or move its pin.

4. **Map the new feature names.** A milestone usually adds `wgpu::FeatureName` values. Add them to `RNWGPU_FOR_EACH_FEATURE_NAME` in `cpp/rnwgpu/api/GPUFeatures.h`, which is the single list both conversion directions are generated from; `src/__tests__/FeatureNames.spec.ts` diffs it against the installed `webgpu_cpp.h` and fails when one is missing.

5. **Update the compatibility table** in `apps/docs/content/docs/integrations/react-native-skia.mdx` with the new milestone row, so users can pair react-native-webgpu and `react-native-skia` versions.

6. **Build both platforms.** A milestone can also change Dawn's C++ API surface, not just add features — e.g. `chrome-m154` turned `SharedTextureMemory::BeginAccess`/`EndAccess`, `Adapter::GetLimits`, `Device::GetLimits`, and `SharedTextureMemory::GetProperties` from a bool-ish return into `wgpu::Status` (no implicit bool conversion), breaking every `if (!result)` / `if (result)` call site in `cpp/rnwgpu/api/*.cpp`. Building iOS and Android is the way these surface; fix by comparing explicitly (`result == wgpu::Status::Success`).

7. **Verify and commit.** Build and run the example app (see "Development workflow" above for reaching the Tests screen), then commit the release-pin update.

## Swift Package Manager (preview)

CocoaPods stays the default. `Package.swift` is additive: SwiftPM ignores the
podspec, and CocoaPods ignores `Package.swift`.

Automatic SwiftPM preparation requires **React Native 0.87.1 or newer**, whose
plugin contract includes `scriptPhases`. Keep CocoaPods on earlier versions.
The `spm-example/` harness keeps its React Native dependencies separate from
the workspace. Prepare headers with the command above before setting it up.
Its [README](../../spm-example/README.md) covers the details; the short form is:

```sh
cd spm-example && npm install
cd ios && npx react-native spm update
xcodebuild -project SpmExample.xcodeproj -scheme SpmExample \
  -configuration Debug -sdk iphonesimulator \
  -destination 'generic/platform=iOS Simulator' CODE_SIGNING_ALLOWED=NO build
```

The manifest mirrors `react-native-skia`'s `packages/skia/Package.swift`
on purpose: same relative paths to React Native's header products, same
`.iOS(.v15)` floor, same `RCT_NEW_ARCH_ENABLED` / `RCT_REMOVE_LEGACY_ARCH` and
`DEBUG` / `NDEBUG` defines. When one changes, change the other.

`spm-plugin.js` explicitly registers the package and product with RN's
`Autolinked` aggregate. It references the installed package through a symlink at
`<app>/ios/build/generated/autolinking/libs/ReactNativeWebGPU`, and SwiftPM
resolves the manifest's relative paths against that symlink rather than against
`packages/webgpu`. The two React Native package paths are therefore identical
for every standard app. The target name is pinned in `react-native.config.js`;
without it a future React Native release would derive it from the podspec
instead and change the header import prefix. Autolinking recreates this
generated symlink; after moving a checkout, rerun `spm update` from `ios/`.

The platform floor must not exceed the one in React Native's generated
`Autolinked` aggregate, which is hardcoded to iOS 15.0 on 0.87.1: SwiftPM
refuses to link a product whose floor is above the depending target's.
react-native#58379 derives the aggregate's floor from the app instead; until it
ships, `.iOS(.v15)` is the only value that links.

#### Dawn

The plugin generates `build/generated/autolinking/ReactNativeWebGPUBinaries`,
a local C package carrying the static library link settings, and adds an app
phase before compilation. The native manifest uses bundled headers, so package
resolution downloads no Dawn binary or all-platform XCFramework.

The Xcode phase follows the same steps as CocoaPods; see [Apple binary preparation](#how-apple-binaries-are-prepared).
Set **User Script Sandboxing** to **NO** on the app target. CocoaPods configures
this on its pod target automatically. Refresh `spm update` after changing the
package or pin so generated settings match the installed package.

### How Apple binaries are prepared

This section explains what the scripts do automatically during setup and app builds (both CocoaPods and SwiftPM). No extra manual steps are required.

0. `pod install` or `react-native spm update` registers the Xcode build phase.

   CocoaPods or the RN 0.87.1+ SwiftPM plugin (`spm-plugin.js`) adds the phase automatically.
   The phase runs `prepare-apple-binaries.sh`, which sources `apple-binaries-utils.sh`.
   No binaries are downloaded during setup.

During each Apple app build, the script:

1. Selects the SDK, platform variant, and required architectures from Xcode.
2. Uses local overrides or downloads the selected SDK's Dawn archive.
3. Verifies pinned checksums and SDK metadata before use.
4. Updates the selected XCFramework slice under `$OBJROOT/ReactNativeWebGPUBinaries`.
5. Stages `librnwgpu_webgpu_dawn.a` in build products for static linking.
6. Reuses unchanged downloads, slices, and libraries on later builds.

- Downloads share `~/Library/Caches/react-native-skia`, similar to React Native and Hermes handling.
- Set `RNSKIA_CACHE_DIR` to change the cache location.
- Local overrides use `libs/apple/<sdk>` with `.a` libraries and `metadata.json`; symlink `libs/apple` to Skia's `libs/dawn/apple` to test local Dawn builds.
- WebGPU always supplies Dawn; shared Skia integration requires matching release versions and Dawn commits in both packages' `binaries.json`.
- Expand **Prepare React Native WebGPU binaries** in Xcode's report navigator for errors.

See [SwiftPM Dawn](#dawn) for SwiftPM integration.
