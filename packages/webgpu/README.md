# React Native WebGPU

WebGPU for React Native, powered by Dawn.

[wcandillon.github.io/react-native-webgpu](https://wcandillon.github.io/react-native-webgpu/)

## Getting Started

[Installation instructions](https://wcandillon.github.io/react-native-webgpu/docs/getting-started/installation)

## Expo config plugin

The package ships with an Expo config plugin that disables Metal API validation in the iOS Xcode scheme, which is required to run WebGPU on the iOS Simulator (Metal API validation reports false positives for Dawn that do not occur on device). Because the plugin is part of the package, plugin and package versions are always aligned.

```json
{
  "expo": {
    "plugins": ["react-native-webgpu"]
  }
}
```

See the [Expo guide](https://wcandillon.github.io/react-native-webgpu/docs/getting-started/expo#config-plugin) for details.

## Contributing

For detailed information on library development, building, testing, and contributing guidelines, please see [CONTRIBUTING.md](packages/webgpu/CONTRIBUTING.md).