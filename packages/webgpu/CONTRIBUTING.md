# Contributing

## Development workflow

```sh
yarn
```

Dawn, the WebGPU implementation, comes with the `react-native-webgpu-dawn` dependency of `packages/webgpu`: the Android shared libraries, the Apple xcframework and the headers. Nothing to download or build.

Example app (from `apps/example/`): `yarn start` · `yarn ios` · `yarn android`

From the repo root: `yarn lint` · `yarn tsc` · `yarn build:docs`

Tests (from `packages/webgpu/`): `yarn test:ref` (Chrome reference) · `yarn test` (E2E) · `yarn test:plugin` (Expo config plugin unit tests)

`yarn test` needs the example app open on its "Tests" screen and connected to Metro first: start Metro with `CI=true yarn start` (from `apps/example/`) — `initialRouteName` in `apps/example/src/App.tsx` is `"Tests"` when `process.env.CI === "true"`, inlined at bundle time via the `transform-inline-environment-variables` babel plugin — then launch the app (`yarn ios` / `yarn android`, or install+launch an existing build). The screen shows "Connecting to localhost..." once it's ready; `yarn test` then picks it up automatically. If a run fails mid-suite, reload the app (`curl -X POST http://localhost:8081/reload`) before the next run — a broken `GPUDevice` from the failed run makes unrelated later tests fail too. The app and the test server talk over port 4242 by default; set `E2E_PORT` for both Metro (it is inlined into the bundle, so start Metro with `--reset-cache` after changing it) and `yarn test` to use another one, as CI does to keep clear of the other projects sharing its runner.

Other `packages/webgpu` scripts: `yarn check-dawn` (checks the installed Dawn against the C++ workarounds pinned to a Dawn commit; CI runs it after `yarn install`)

## Expo config plugin

The Expo config plugin lives in `plugin/src` and is compiled to `plugin/build` by `yarn build:plugin` (also part of `yarn build`, so releases always ship it). `app.plugin.js` at the package root is the entry point Expo resolves when apps list `"react-native-webgpu"` in their `plugins`. The plugin disables Metal API validation in the generated iOS Xcode scheme (required on the iOS Simulator); it is released together with the package by the regular release workflow, so plugin and package versions are always aligned.

## Upgrading Dawn

Dawn ships in the `react-native-webgpu-dawn` npm package, which react-native-skia builds and publishes together with its own Skia Graphite binaries (the **Build and publish binaries** workflow of [wcandillon/react-native-skia](https://github.com/wcandillon/react-native-skia), see its `packages/skia/CONTRIBUTING.md`). It is built from the Dawn commit the Skia release pins in its DEPS, with the patches that release needs, so the two libraries always link the same Dawn. The package records that commit in its `package.json` (`dawn.commit`) and its version follows the Skia one (`154.2.0` for Skia `m154_8037_58b`).

`packages/webgpu/package.json` pins it exactly (`dependencies`), and keeps the `dawn` label (`chrome-m154a`) that react-native-skia 3.0.x compares against its own Dawn at `pod install` time; it goes away once react-native-skia depends on the package too.

Steps to bump to a new Dawn (new Skia milestone `m<N>`):

1. **Wait for the binaries.** react-native-skia publishes `react-native-webgpu-dawn` at the new version.

2. **Bump the dependency** in `packages/webgpu/package.json` and run `yarn`.

3. **Review the workarounds.** `yarn check-dawn` fails when the Dawn commit changed while a workaround in `scripts/check-dawn.ts` is still pinned to the previous one: check whether the new Dawn carries the upstream fix, then drop the workaround or move its pin.

4. **Map the new feature names.** A milestone usually adds `wgpu::FeatureName` values. Add them to `RNWGPU_FOR_EACH_FEATURE_NAME` in `cpp/rnwgpu/api/GPUFeatures.h`, which is the single list both conversion directions are generated from; `src/__tests__/FeatureNames.spec.ts` diffs it against the installed `webgpu_cpp.h` and fails when one is missing.

5. **Update the compatibility table** in `apps/docs/content/docs/integrations/react-native-skia.mdx` with the new milestone row, so users can pair react-native-webgpu and `react-native-skia` versions.

6. **Build both platforms.** A milestone can also change Dawn's C++ API surface, not just add features — e.g. `chrome-m154` turned `SharedTextureMemory::BeginAccess`/`EndAccess`, `Adapter::GetLimits`, `Device::GetLimits`, and `SharedTextureMemory::GetProperties` from a bool-ish return into `wgpu::Status` (no implicit bool conversion), breaking every `if (!result)` / `if (result)` call site in `cpp/rnwgpu/api/*.cpp`. Building iOS and Android is the way these surface; fix by comparing explicitly (`result == wgpu::Status::Success`).

7. **Verify and commit.** Build and run the example app (see "Development workflow" above for reaching the Tests screen), then commit the dependency bump.
## Swift Package Manager (preview)

CocoaPods stays the default. `Package.swift` is additive: SwiftPM ignores the
podspec, and CocoaPods ignores `Package.swift`.

SwiftPM support requires **React Native 0.87 or newer**; earlier releases ship
no `scripts/spm`. `apps/example` is on an older version, so it cannot exercise
this path. The harness is `spm-example/` at the repo root, deliberately outside
the yarn workspace so its React Native does not collide with the workspace's.
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

Autolinking references the library through a symlink at
`<app>/ios/build/generated/autolinking/libs/ReactNativeWebGPU`, and SwiftPM
resolves the manifest's relative paths against that symlink rather than against
`packages/webgpu`. The two React Native package paths are therefore identical
for every standard app. The target name is pinned in `react-native.config.js`;
without it a future React Native release would derive it from the podspec
instead and change the header import prefix.

The platform floor must not exceed the one in React Native's generated
`Autolinked` aggregate, which is hardcoded to iOS 15.0 on 0.87.1: SwiftPM
refuses to link a product whose floor is above the depending target's.
react-native#58379 derives the aggregate's floor from the app instead; until it
ships, `.iOS(.v15)` is the only value that links.

#### Dawn

The manifest depends on the `react-native-webgpu-dawn` package by path (a
sibling in `node_modules`, or this monorepo's root) and links its
`libwebgpu_dawn.xcframework`, whose slices embed the `webgpu/` and `dawn/`
headers, so no header search path is needed for them. A Graphite build of
react-native-skia installed alongside must link the same Dawn, which the
manifest checks at evaluation time: a react-native-skia that depends on
`react-native-webgpu-dawn` must pin the same version, and the 3.0.x line, which
vendors its own Dawn, must record this package's `dawn` tag at
`react-native-skia-graphite-apple-ios/libs/.dawn-version` (hoisted or nested
under `react-native-skia/node_modules`, also checked for macOS and for the v2
Graphite previews at `@shopify/react-native-skia/libs/.dawn-version`). SwiftPM
caches manifest evaluations, so after changing either package's Dawn reset the
package caches if the check does not re-run.

After changing which binaries a checkout uses, delete
`ios/<App>.xcodeproj/project.xcworkspace/xcshareddata/swiftpm/Package.resolved`:
a stale pin silently keeps the previous source.
