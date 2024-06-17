# React Native WGPU

React Native implementation of WebGPU using [Dawn](https://dawn.googlesource.com/dawn).

## Library Development

Make sure to check out the sub modules:

```
git submodule update --init --recursive
```

Make sure you have all the tools required for building the skia libraries (Android Studio, XCode, Ninja, CMake, Android NDK / build tools).

### Building 

* `npm run build-dawn`

### Upgrading

1. `git submodule update --recursive --remote`
2. `npm run clean-dawn`
3. `npm run build-dawn`