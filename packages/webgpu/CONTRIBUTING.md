# Contributing

Contributions are always welcome, no matter how large or small!

We want this community to be friendly and respectful to each other. Please follow it in all your interactions with the project. Before contributing, please read the [code of conduct](./CODE_OF_CONDUCT.md).

## Development workflow

This project is a monorepo managed using [Yarn workspaces](https://yarnpkg.com/features/workspaces). It contains the following packages:

- The library package in the root directory.
- An example app in the `example/` directory.

To get started with the project, run `yarn` in the root directory to install the required dependencies for each package:

```sh
yarn
```

> Since the project relies on Yarn workspaces, you cannot use [`npm`](https://github.com/npm/cli) for development.

The [example app](/example/) demonstrates usage of the library. You need to run it to test any changes you make.

It is configured to use the local version of the library, so any changes you make to the library's source code will be reflected in the example app. Changes to the library's JavaScript code will be reflected in the example app without a rebuild, but native code changes will require a rebuild of the example app.

If you want to use Android Studio or XCode to edit the native code, you can open the `example/android` or `example/ios` directories respectively in those editors. To edit the Objective-C or Swift files, open `example/ios/WgpuExample.xcworkspace` in XCode and find the source files at `Pods > Development Pods > react-native-wgpu`.

To edit the Java or Kotlin files, open `example/android` in Android studio and find the source files at `react-native-wgpu` under `Android`.

You can use various commands from the root directory to work with the project.

To start the packager:

```sh
yarn example start
```

To run the example app on Android:

```sh
yarn example android
```

To run the example app on iOS:

```sh
yarn example ios
```

By default, the example is configured to build with the old architecture. To run the example with the new architecture, you can do the following:

1. For Android, run:

   ```sh
   ORG_GRADLE_PROJECT_newArchEnabled=true yarn example android
   ```

2. For iOS, run:

   ```sh
   cd example/ios
   RCT_NEW_ARCH_ENABLED=1 pod install
   cd -
   yarn example ios
   ```

If you are building for a different architecture than your previous build, make sure to remove the build folders first. You can run the following command to cleanup all build folders:

```sh
yarn clean
```

To confirm that the app is running with the new architecture, you can check the Metro logs for a message like this:

```sh
Running "WgpuExample" with {"fabric":true,"initialProps":{"concurrentRoot":true},"rootTag":1}
```

Note the `"fabric":true` and `"concurrentRoot":true` properties.

Make sure your code passes TypeScript and ESLint. Run the following to verify:

```sh
yarn typecheck
yarn lint
```

To fix formatting errors, run the following:

```sh
yarn lint --fix
```

Remember to add tests for your change if possible. Run the unit tests by:

```sh
yarn test
```

### Commit message convention

We follow the [conventional commits specification](https://www.conventionalcommits.org/en) for our commit messages:

- `fix`: bug fixes, e.g. fix crash due to deprecated method.
- `feat`: new features, e.g. add new method to the module.
- `refactor`: code refactor, e.g. migrate from class components to hooks.
- `docs`: changes into documentation, e.g. add usage example for the module..
- `test`: adding or updating tests, e.g. add integration tests using detox.
- `chore`: tooling changes, e.g. change CI config.

Our pre-commit hooks verify that your commit message matches this format when committing.

### Linting and tests

[ESLint](https://eslint.org/), [Prettier](https://prettier.io/), [TypeScript](https://www.typescriptlang.org/)

We use [TypeScript](https://www.typescriptlang.org/) for type checking, [ESLint](https://eslint.org/) with [Prettier](https://prettier.io/) for linting and formatting the code, and [Jest](https://jestjs.io/) for testing.

Our pre-commit hooks verify that the linter and tests pass when committing.

### Publishing to npm

We use [release-it](https://github.com/release-it/release-it) to make it easier to publish new versions. It handles common tasks like bumping version based on semver, creating tags and releases etc.

To publish new versions, run the following:

```sh
yarn release
```

### Scripts

The `package.json` file contains various scripts for common tasks:

- `yarn`: setup project by installing dependencies.
- `yarn typecheck`: type-check files with TypeScript.
- `yarn lint`: lint files with ESLint.
- `yarn test`: run unit tests with Jest.
- `yarn example start`: start the Metro server for the example app.
- `yarn example android`: run the example app on Android.
- `yarn example ios`: run the example app on iOS.

### Sending a pull request

> **Working on your first pull request?** You can learn how from this _free_ series: [How to Contribute to an Open Source Project on GitHub](https://app.egghead.io/playlists/how-to-contribute-to-an-open-source-project-on-github).

When you're sending a pull request:

- Prefer small pull requests focused on one change.
- Verify that linters and tests are passing.
- Review the documentation to make sure it looks good.
- Follow the pull request template when opening a pull request.
- For pull requests that change the API or implementation, discuss with maintainers first by opening an issue.

## Bringing your own Dawn

By default `react-native-wgpu` bundles its own [Dawn](https://dawn.googlesource.com/dawn) build (`libwebgpu_dawn.xcframework` on Apple, `libwebgpu_dawn.so` on Android) and creates its own `wgpu::Instance` at startup.

When another module in the same app already links a Dawn build (for example React Native Skia with the Graphite backend), shipping a second Dawn is wasteful and, more importantly, the two Dawn runtimes cannot share resources: a `wgpu::Device` created by one cannot be used by the other. The app should contain exactly one Dawn, owned by a single module, and `react-native-wgpu` should consume it.

"Bring your own Dawn" (BYO) mode supports this. It has two halves: passing the host's instance to the bindings at runtime, and skipping the bundled Dawn at build time.

> **Dawn version must match.** In BYO mode the bindings compile against the host's Dawn headers and resolve against the host's Dawn binary. `webgpu_cpp.h` is not ABI-stable across Dawn revisions, so the host's Dawn revision must match the one this package targets (see the `dawn` field in `package.json`). If they differ, align the versions first.

### Runtime: provide the `wgpu::Instance`

`GPU` has a constructor that adopts an externally-owned instance instead of creating its own:

```cpp
// cpp/rnwgpu/api/GPU.h
explicit GPU(jsi::Runtime &runtime);                  // creates and owns a default Dawn instance
GPU(jsi::Runtime &runtime, wgpu::Instance instance);  // uses an externally-owned instance
```

The host installs the WebGPU bindings by constructing `GPU` with its own instance, for example:

```cpp
auto gpu = std::make_shared<rnwgpu::GPU>(runtime, hostDawnContext.getWGPUInstance());
```

The caller keeps ownership of the instance and must keep it alive for the lifetime of the `GPU` object.

### Build: skip the bundled Dawn

#### iOS / Apple

Set environment variables before `pod install`:

| Variable                   | Effect                                                                                              |
| -------------------------- | -------------------------------------------------------------------------------------------------- |
| `RN_WEBGPU_EXTERNAL_DAWN`  | `1` skips bundling `libwebgpu_dawn.xcframework`.                                                    |
| `RN_WEBGPU_DAWN_HEADERS`   | Directory containing the `webgpu/` header folder (Dawn's include dir). Prepended to header search.  |

```sh
RN_WEBGPU_EXTERNAL_DAWN=1 \
RN_WEBGPU_DAWN_HEADERS=/abs/path/to/host/dawn/include \
pod install
```

Our pod compiles to a static library, so the undefined Dawn symbols are resolved from the host's framework when the final app binary is linked. There is nothing extra to link explicitly.

#### Android

Set Gradle properties (e.g. in the app's `gradle.properties`):

| Property            | Effect                                                                                                   |
| ------------------- | ------------------------------------------------------------------------------------------------------- |
| `wgpuExternalDawn`  | `true` skips bundling `libwebgpu_dawn.so`.                                                               |
| `wgpuDawnHeaders`   | Directory containing the `webgpu/` header folder (Dawn's include dir). Prepended to the include search.  |
| `wgpuDawnLib`       | Optional. Path to the host's Dawn `.so` to link against. If omitted, Dawn symbols are resolved at load time from the host. |

```properties
wgpuExternalDawn=true
wgpuDawnHeaders=/abs/path/to/host/dawn/include
wgpuDawnLib=/abs/path/to/host/libwebgpu_dawn.so
```

These are forwarded to CMake as `RN_WEBGPU_EXTERNAL_DAWN`, `RN_WEBGPU_DAWN_HEADERS`, and `RN_WEBGPU_DAWN_LIB`. When `wgpuDawnLib` is not set, the build allows undefined Dawn symbols (`-Wl,--allow-shlib-undefined`) and relies on the host's `.so` being loaded first with its symbols visible; passing `wgpuDawnLib` is the more robust option.

Leaving these flags unset keeps the default behavior: `react-native-wgpu` bundles and links its own Dawn exactly as before.
