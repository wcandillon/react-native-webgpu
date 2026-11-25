# macOS example

The runner reuses `../example/src/App.tsx` and the shared gallery. Its RN macOS
0.83 dependencies stay separate from the mobile RN 0.88 app. React 19.2,
Reanimated 4.6 and Worklets 0.12 match the desktop renderer; Metro and Babel
resolve shared imports to that same dependency graph.

## Build and run

From the repository root:

```sh
yarn install
yarn workspace example-macos bundle:macos
yarn workspace example-macos pod:install:macos
yarn workspace example-macos build:macos
```

The app targets macOS 14.0. Dawn is prepared from the pinned SDK archive during Xcode compilation;
nothing is built from Dawn sources. For development, run these in separate terminals:

```sh
yarn workspace example-macos start
yarn workspace example-macos macos
```

The shared gallery already uses a JavaScript navigation stack. Screens stays on
4.19 for its desktop View fallback. The camera integration remains mobile-only;
its native Nitro dependencies are not installed in this workspace. Gallery rows
use React Native's `TouchableOpacity`, matching Skia's example. Gesture Handler
buttons stopped accepting mouse clicks after returning from a demo on macOS;
white rows also set black text so dark appearance cannot hide their labels.

The RN macOS patch keeps Codegen on the desktop native modules and lets its
file request handler read bundled images. Without the latter, bridgeless
`fetch(asset.uri)` has no handler for PNG/JPEG assets, leaving the gallery
waiting for its textures.

## CI

Every PR bundles the macOS gallery on Linux. Changes to the desktop runner,
native sources or dependency configuration also build the macOS app on a hosted
Mac, matching Skia's platform-specific CI checks.
