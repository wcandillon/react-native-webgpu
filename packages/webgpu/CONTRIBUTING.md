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

Tests (from `packages/webgpu/`): `yarn test:ref` (Chrome reference) · `yarn test` (E2E — open the example app on the E2E screen) · `yarn test:plugin` (Expo config plugin unit tests)

Other `packages/webgpu` scripts: `yarn clean-dawn` · `yarn build-dawn`

## Expo config plugin

The Expo config plugin lives in `plugin/src` and is compiled to `plugin/build` by `yarn build:plugin` (also part of `yarn build`, so releases always ship it). `app.plugin.js` at the package root is the entry point Expo resolves when apps list `"react-native-webgpu"` in their `plugins`. The plugin disables Metal API validation in the generated iOS Xcode scheme (required on the iOS Simulator); it is released together with the package by the regular release workflow, so plugin and package versions are always aligned.

## Upgrading Dawn

The Dawn version tracks the one shipped by `@shopify/react-native-skia` Graphite builds: the pin is the exact Dawn commit from the Skia milestone's DEPS file (`third_party/externals/dawn` in Skia's DEPS). It is recorded in two places that must stay in sync:

- the `externals/dawn` submodule gitlink (the commit the submodule points at)
- `packages/webgpu/package.json` → `"dawn"` (a human-readable label, e.g. `chrome-m150`; Skia milestones mirror Chrome milestones) and `"dawnCommit"` (the exact commit hash)

The **Build Dawn** workflow verifies the gitlink matches `dawnCommit` and fails otherwise.

`yarn install-dawn` downloads **prebuilt** binaries from a GitHub release on this repo tagged `dawn-<version-slug>` (e.g. `dawn-chrome-m150`). `yarn build-dawn` builds the same binaries from the submodule source instead.

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

5. **Verify and commit.** Build and run the example app, then commit the submodule bump together with the updated `package.json`.