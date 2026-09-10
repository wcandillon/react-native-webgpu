# Contributing

## Development workflow

```sh
git submodule update --init
yarn
cd packages/webgpu && yarn install-dawn   # prebuilt Dawn binaries
# or: yarn build-dawn                     # build Dawn from source
```

Example app (from `apps/example/`): `yarn start` · `yarn ios` · `yarn android`

From the repo root: `yarn lint` · `yarn tsc` · `yarn build:docs`

Tests (from `packages/webgpu/`): `yarn test:ref` (Chrome reference) · `yarn test` (E2E) · `yarn test:plugin` (Expo config plugin unit tests)

`yarn test` needs the example app open on its "Tests" screen and connected to Metro first: start Metro with `CI=true yarn start` (from `apps/example/`) — `initialRouteName` in `apps/example/src/App.tsx` is `"Tests"` when `process.env.CI === "true"`, inlined at bundle time via the `transform-inline-environment-variables` babel plugin — then launch the app (`yarn ios` / `yarn android`, or install+launch an existing build). The screen shows "Connecting to localhost..." once it's ready; `yarn test` then picks it up automatically. If a run fails mid-suite, reload the app (`curl -X POST http://localhost:8081/reload`) before the next run — a broken `GPUDevice` from the failed run makes unrelated later tests fail too.

Other `packages/webgpu` scripts: `yarn clean-dawn` · `yarn build-dawn`

## Expo config plugin

The Expo config plugin lives in `plugin/src` and is compiled to `plugin/build` by `yarn build:plugin` (also part of `yarn build`, so releases always ship it). `app.plugin.js` at the package root is the entry point Expo resolves when apps list `"react-native-webgpu"` in their `plugins`. The plugin disables Metal API validation in the generated iOS Xcode scheme (required on the iOS Simulator); it is released together with the package by the regular release workflow, so plugin and package versions are always aligned.

## Upgrading Dawn

The Dawn version tracks the one shipped by `@shopify/react-native-skia` Graphite builds: the pin is the exact Dawn commit from the Skia milestone's DEPS file (`third_party/externals/dawn` in Skia's DEPS). It is recorded in two places that must stay in sync:

- the `externals/dawn` submodule gitlink (the commit the submodule points at)
- `packages/webgpu/package.json` → `"dawn"` (a human-readable label, e.g. `chrome-m152`; Skia milestones mirror Chrome milestones) and `"dawnCommit"` (the exact commit hash)

The **Build Dawn** workflow verifies the gitlink matches `dawnCommit` and fails otherwise.

`yarn install-dawn` downloads **prebuilt** binaries from a GitHub release on this repo tagged `dawn-<version-slug>` (e.g. `dawn-chrome-m152`). `yarn build-dawn` builds the same binaries from the submodule source instead.

Steps to bump to a new Dawn version (new Skia milestone `m<N>`):

1. **Find the Dawn commit** in the Skia milestone's `DEPS` file (`third_party/externals/dawn` entry).

2. **Point the submodule at that commit** and update `package.json` (`"dawn": "chrome-m<N>"`, `"dawnCommit": "<hash>"`):

   ```sh
   cd externals/dawn && git fetch origin && git checkout <hash> && cd ../..
   ```

3. **Publish prebuilt binaries.** Trigger the **Build Dawn** workflow (`.github/workflows/build-dawn.yml`, `workflow_dispatch`). It builds Android + Apple from the submodule and creates the `dawn-chrome-m<N>` release with the headers, the Android `.so`s, and the Apple `.xcframework`. (To build locally instead, run `yarn build-dawn`; this requires the Android NDK and Xcode toolchains.)

4. **Pull the new binaries** once the release exists:

   ```sh
   cd packages/webgpu && yarn install-dawn
   ```

5. **Map the new feature names.** A milestone usually adds `wgpu::FeatureName` values. Add them to `RNWGPU_FOR_EACH_FEATURE_NAME` in `cpp/rnwgpu/api/GPUFeatures.h`, which is the single list both conversion directions are generated from; `src/__tests__/FeatureNames.spec.ts` diffs it against the installed `webgpu_cpp.h` and fails when one is missing.

6. **Update the compatibility table** in `apps/docs/content/docs/integrations/react-native-skia.mdx` with the new milestone row, so users can pair react-native-webgpu and `@shopify/react-native-skia` versions.

7. **Build both platforms.** A milestone can also change Dawn's C++ API surface, not just add features — e.g. `chrome-m154` turned `SharedTextureMemory::BeginAccess`/`EndAccess`, `Adapter::GetLimits`, `Device::GetLimits`, and `SharedTextureMemory::GetProperties` from a bool-ish return into `wgpu::Status` (no implicit bool conversion), breaking every `if (!result)` / `if (result)` call site in `cpp/rnwgpu/api/*.cpp`. Building iOS and Android is the way these surface; fix by comparing explicitly (`result == wgpu::Status::Success`).

8. **Verify and commit.** Build and run the example app (see "Development workflow" above for reaching the Tests screen), then commit the submodule bump together with the updated `package.json`.