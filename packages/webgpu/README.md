# React Native WebGPU

WebGPU for React Native, powered by [Dawn](https://dawn.googlesource.com/dawn).

[wcandillon.github.io/react-native-webgpu](https://wcandillon.github.io/react-native-webgpu/)

## Getting Started

[Installation instructions](https://wcandillon.github.io/react-native-webgpu/docs/getting-started/installation)

## Contributing

For detailed information on library development, building, testing, and contributing guidelines, please see [CONTRIBUTING.md](packages/webgpu/CONTRIBUTING.md).



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
