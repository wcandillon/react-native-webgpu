# RenderEffect PoC (Path B)

A minimal Expo app proving **Path B**: attaching a GPU compositor shader to live
native scrolling content, so the effect re-runs every frame on the
compositor with no readback and no per-frame work from JS. This is the
"real-time" complement to the `copyElementImageToTexture` capture path (Path A)
in `apps/example`.

The same effect (vignette + edge chromatic aberration) is written twice:

- **iOS:** a SwiftUI `.layerEffect` with a Metal `[[stitchable]]` shader
  (`modules/render-effect/ios/Shaders.metal`). Requires **iOS 17+**.
- **Android:** `View.setRenderEffect` + a `RuntimeShader` (AGSL,
  `modules/render-effect/android/.../RenderEffectView.kt`). Requires **API 33+**.

Both live in a local Expo module at `modules/render-effect` (autolinked).

## Two ways the iOS effect is wired

1. **React-Native-authored content (current `App.tsx`)** — the views are written
   in JSX with `@expo/ui` primitives (`Host` / `VStack` / `Text`), which render as
   real SwiftUI. The Metal shader is exposed as a **custom `@expo/ui` view
   modifier** (`layerEffectShader`, registered via `ViewModifierRegistry` in
   `RenderEffectModule.swift`) and applied in the same SwiftUI tree:
   `<VStack modifiers={[layerEffectShader()]}>`. This needs Expo **SDK 56+**
   (the custom-modifier API) and `@expo/ui`.
2. **Standalone SwiftUI view (`RenderEffectView`)** — the original
   `RenderEffectView` ExpoView hosts a hand-written SwiftUI list with the same
   `.layerEffect`. Kept for reference.

> Note: `.layerEffect` only samples genuine SwiftUI content, never live
> UIKit/RN views, which is why the RN-authored path goes through `@expo/ui`
> (SwiftUI), not plain `<View>`/`<Text>`.

## Run

From the repo root:

```sh
yarn install
cd apps/expo-example
yarn prebuild --clean    # regenerates ios/ + android/ with the @expo/ui pod graph
yarn ios                 # iOS 17+ simulator or device
yarn android             # API 33+ device/emulator
```

If you only changed Swift/Metal (not the podspec), `yarn ios` is enough; a
podspec change (e.g. the `ExpoUI` dependency) needs the `prebuild --clean` above
or a `cd ios && pod install`.

## Known iOS caveat (may need one build-time fix)

SwiftUI's `Shader` resolves its Metal function through `ShaderLibrary`. This PoC
uses `ShaderLibrary.default`, which reads the app's main-bundle `default.metallib`.
A `.metal` file inside a static CocoaPods pod (this module) usually compiles into
the app's `default.metallib`, so `ShaderLibrary.default.vignetteAberration`
resolves. If at runtime the shader function is not found:

1. Move `Shaders.metal` into the app target (Xcode → add to the app target's
   "Compile Sources"), or
2. Add a `resource_bundle` to `ios/RenderEffect.podspec` and load it with
   `ShaderLibrary(url:)` from that bundle instead of `.default`.

This is the one piece that can only be confirmed by an actual iOS build.
