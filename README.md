# React Native WebGPU

WebGPU for React Native, powered by [Dawn](https://dawn.googlesource.com/dawn). React Native ≥ 0.81, New Architecture only.

**Documentation:** [wcandillon.github.io/react-native-webgpu](https://wcandillon.github.io/react-native-webgpu/)

```sh
npm install react-native-webgpu
```

Expo: `npx create-expo-app@latest -e with-webgpu`

Example app: [`apps/example/`](apps/example/)

## Contributing

Yarn workspaces monorepo — use `yarn`, not `npm`.

```sh
git submodule update --init
yarn
cd packages/webgpu && yarn install-dawn   # prebuilt Dawn binaries
# or: yarn build-dawn                     # build Dawn from source
```

Example app (from `apps/example/`): `yarn start` · `yarn ios` · `yarn android`

From the repo root: `yarn lint` · `yarn tsc` · `yarn build:docs`

Tests (from `packages/webgpu/`): `yarn test:ref` (Chrome reference) · `yarn test` (E2E — open the example app on the E2E screen)

Other `packages/webgpu` scripts: `yarn codegen` · `yarn clean-dawn` · `yarn build-dawn`

See also [`packages/webgpu/CONTRIBUTING.md`](packages/webgpu/CONTRIBUTING.md).
