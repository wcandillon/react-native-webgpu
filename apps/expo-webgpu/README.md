# Expo WebGPU template

This template demonstrates three ways to use
[react-native-webgpu](https://github.com/wcandillon/react-native-webgpu) with Expo:

- the WebGPU API and WGSL directly;
- imperative Three.js through `three/webgpu`;
- declarative React Three Fiber.

## Create an app

```sh
npx create-expo-app@latest my-webgpu-app --template https://github.com/wcandillon/react-native-webgpu/tree/main/apps/expo-webgpu
cd my-webgpu-app
npx expo run
```

The native examples require a development build and do not run in Expo Go.

The Metro resolver redirects Three.js to its WebGPU build and selects React
Three Fiber's standard module build on native platforms. React Native requires
`context.present()` after submitting or rendering a frame.
