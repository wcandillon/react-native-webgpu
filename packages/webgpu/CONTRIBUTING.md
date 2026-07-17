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

The Dawn version is pinned in two places that must stay in sync:

- `.gitmodules` → `submodule.externals/dawn.branch` (e.g. `chromium/7849`)
- `packages/webgpu/package.json` → the `"dawn"` field (same value, e.g. `chromium/7849`)

`yarn install-dawn` downloads **prebuilt** binaries from a GitHub release tagged `dawn-<branch-slug>` (e.g. `dawn-chromium-7849`); the release host is configured at the top of `scripts/install-dawn.ts`. `yarn build-dawn` builds the same binaries from the submodule source instead.

Steps to bump to a new Dawn version (`chromium/<N>`):

1. **Point the submodule at the new branch.** Update both `.gitmodules` and the `"dawn"` field in `package.json` to `chromium/<N>`, then move the submodule to the new tip:

   ```sh
   git submodule set-branch --branch chromium/<N> externals/dawn
   git submodule update --remote externals/dawn
   ```

2. **Publish prebuilt binaries.** Trigger the **Build Dawn** workflow (`.github/workflows/build-dawn.yml`, `workflow_dispatch`). It reads the branch from `.gitmodules`, builds Android + Apple, and creates the `dawn-chromium-<N>` release with the headers, the Android `.so`s, and the Apple `.xcframework`. (To build locally instead, run `yarn build-dawn`; this requires the Android NDK and Xcode toolchains.)

3. **Pull the new binaries** once the release exists:

   ```sh
   cd packages/webgpu && yarn install-dawn
   ```

4. **Verify and commit.** Build and run the example app, then commit the submodule bump together with the updated `.gitmodules` and `package.json`.