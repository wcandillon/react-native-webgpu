# Contributing

Thank you for your interest in contributing to React Native WebGPU!

This library is a React Native implementation of WebGPU built on top of Google's [Dawn](https://dawn.googlesource.com/dawn) engine. This guide covers how to set up the project, build the native dependencies, and run the test suite.

## Library Development

To develop react-native-webgpu, you can build Dawn on your computer. Alternatively, you can use the pre-built binaries, which is the fastest way to get started.

In both cases, start by checking out the submodules and installing the JavaScript dependencies:

```sh
git submodule update --init
yarn
```

### Using pre-built binaries

You can install the pre-built Dawn binaries from GitHub instead of building Dawn yourself:

```sh
cd packages/webgpu
yarn install-dawn
```

### Building

If you prefer to build Dawn locally, make sure you have all the tools required for building it: Android Studio, Xcode, [Ninja](https://ninja-build.org/), [CMake](https://cmake.org/), and the Android NDK and build tools.

If you have Android Studio installed, make sure `$ANDROID_NDK` is available, for instance `ANDROID_NDK=/Users/username/Library/Android/sdk/ndk/<version>`. If the NDK is not installed, you can install it via Android Studio under _File > Project Structure > SDK Location_, which shows the NDK path or the option to download it.

```sh
cd packages/webgpu
yarn build-dawn # this can take a while
```

### Upgrading

When a new version of Dawn is included in an upgrade of this library, perform the following steps:

1. Update submodules: `git submodule update --remote`
2. Clean Dawn: `yarn clean-dawn`
3. Build Dawn: `yarn build-dawn`

(Run these from the `packages/webgpu` folder.)

### Codegen

Some of the native bindings are generated. To regenerate them:

```sh
cd packages/webgpu
yarn codegen
```

## Running the Example App

The [example app](/apps/example/) demonstrates every feature and integration of the library, and is where the end-to-end tests run.

From a checkout with Dawn installed (see above):

```sh
cd apps/example
yarn ios     # or: yarn android, yarn macos
```

### iOS Simulator

To run on the iOS Simulator, you need to disable Metal's API validation, which Dawn does not pass cleanly. In Xcode, open **Edit Scheme** and uncheck **Metal Validation**. Learn more in Apple's documentation on [validating your app's Metal API usage](https://developer.apple.com/documentation/xcode/validating-your-apps-metal-api-usage/).

## Testing

When making contributions, testing is an important part of the process. Several scripts are set up in the `packages/webgpu` folder to help you maintain the quality of the codebase:

- `yarn lint` — Lints the code for potential errors and to ensure consistency with our coding standards.
- `yarn tsc` — Runs the TypeScript compiler to check for typing issues.
- `yarn test:ref` — Runs the test suite against Chrome for reference.
- `yarn test` — Runs the end-to-end tests against the example app.

These can also be run for the whole monorepo from the root with `yarn lint`, `yarn tsc`, and `yarn build` (which use [Turborepo](https://turbo.build/)).

### Running End-to-End Tests

The end-to-end tests run against the example app:

1. Start the example app and open the **e2e** screen:

   ```sh
   cd apps/example
   yarn ios # or yarn android
   ```

   By default, the app tries to connect to a test server on `localhost`. To run the suite on a physical device, update the address in [`apps/example/src/useClient.ts`](/apps/example/src/useClient.ts).

2. With the app running on the e2e screen, run the test suite from the `packages/webgpu` folder:

   ```sh
   yarn test
   ```

This verifies that your changes have not introduced any regressions.

## Documentation

The documentation site lives in [`apps/docs`](/apps/docs/) and is built with [Docusaurus](https://docusaurus.io/). To work on it:

```sh
yarn workspace docs start   # local dev server
yarn workspace docs build   # production build
```

The site is deployed automatically to [GitHub Pages](https://wcandillon.github.io/react-native-webgpu/) on every push to `main` that touches `apps/docs`.

## Submitting Changes

Before opening a pull request, please make sure that:

- `yarn lint` passes.
- `yarn tsc` passes.
- The end-to-end tests pass for any behavior you changed, and you have added tests for new features.
