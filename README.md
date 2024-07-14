# React Native WebGPU

React Native implementation of WebGPU using [Dawn](https://dawn.googlesource.com/dawn).

## Library Development

Make sure to check out the sub modules:

```
git submodule update --init
```

Make sure you have all the tools required for building the skia libraries (Android Studio, XCode, Ninja, CMake, Android NDK / build tools).

### Building 

* `cd package && yarn`
* `yarn build-dawn`

### Upgrading

1. `git submodule update --remote`
2. `yarn clean-dawn`
3. `yarn build-dawn`

### Codegen

* `cd package && yarn codegen`