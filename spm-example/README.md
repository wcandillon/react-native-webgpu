# SpmExample

A minimal React Native app that consumes `react-native-webgpu` through
**Swift Package Manager** instead of CocoaPods. It exists to prove
`packages/webgpu/Package.swift` actually builds and renders, and it is what the
spm-example CI steps run.

iOS only. CocoaPods remains the supported default for this library; see the
Swift Package Manager section of
[`packages/webgpu/CONTRIBUTING.md`](../packages/webgpu/CONTRIBUTING.md) for the
design notes. The app and its manifest deliberately mirror
`@shopify/react-native-skia`'s `spm-example`, so the two libraries link the
same way.

## Why it sits outside the yarn workspace

Swift Package Manager support requires **React Native 0.87 or newer** — earlier
releases ship no `scripts/spm`. `apps/example` is pinned to an older version, so
this app keeps its own `node_modules` and installs with npm. It ships no
lockfile: `react` and `react-native` are pinned exactly in `package.json`.

`react-native-webgpu` is linked from the workspace with
`file:../packages/webgpu`, so it builds the working tree, not a published release.

## Build and run

```sh
npm install
cd ios
npx react-native spm add --deintegrate   # injects the Swift packages into the .xcodeproj

xcodebuild -project SpmExample.xcodeproj -scheme SpmExample \
  -configuration Release -sdk iphonesimulator \
  -destination 'generic/platform=iOS Simulator' \
  -derivedDataPath build/dd-release CODE_SIGNING_ALLOWED=NO build
```

Release embeds its own JS bundle, so it needs no Metro. For Debug, run
`npm start` first.

## Gotchas

- **Stale `Package.resolved`.** Xcode caches package pins in
  `ios/SpmExample.xcodeproj/project.xcworkspace/xcshareddata/swiftpm/Package.resolved`.
  When switching the Dawn binary between the release zip and a local
  `libs/apple/` build (`yarn generate-package-swift --local`), delete it first
  or the old source is silently kept.
- **`spm update` re-adds an absolute `HERMES_CLI_PATH`.** On React Native
  0.87.1 the injector writes this machine's own path to `hermesc` into
  `project.pbxproj` and `.spm-injected.json`. The build does not need it, so it
  is deliberately not committed. Strip it again before committing:
  ```sh
  sed -i '' '/HERMES_CLI_PATH = /d' ios/SpmExample.xcodeproj/project.pbxproj
  sed -i '' '/^        "HERMES_CLI_PATH",$/d' ios/SpmExample.xcodeproj/.spm-injected.json
  ```
  react/react-native#58292 proposes removing the write upstream. Until that is
  in a React Native release, the strip stays necessary on every commit.
- **The Podfile is not for CocoaPods.** React Native's CLI finds the iOS
  project by searching for a Podfile, so an SwiftPM-only app still has to ship
  one. It installs nothing and raises if `pod install` is ever run.
- **Two React copies.** `metro.config.js` forces `react` and `react-native` to
  this app's own copies. Without that, the Canvas component resolves the
  workspace root's React and hooks fail.
- **Platform floor.** `Package.swift` declares `.iOS(.v15)` and must not go
  higher: React Native 0.87.1's generated `Autolinked` aggregate hardcodes
  15.0 and SwiftPM refuses a product with a higher floor
  (react/react-native#58379 fixes the aggregate).
