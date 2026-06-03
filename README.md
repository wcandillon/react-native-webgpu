# React Native WebGPU

React Native implementation of [WebGPU](https://www.w3.org/TR/webgpu/) using [Dawn](https://dawn.googlesource.com/dawn).

It runs on iOS, Android, macOS, and visionOS, and the same code runs on the Web.

## 📚 Documentation

**Full documentation is available at [wcandillon.github.io/react-native-webgpu](https://wcandillon.github.io/react-native-webgpu/).**

It covers installation requirements, the differences between React Native WebGPU and the Web, and integrations with third-party libraries such as [Three.js](https://wcandillon.github.io/react-native-webgpu/docs/integrations/three-js), [TensorFlow.js](https://wcandillon.github.io/react-native-webgpu/docs/integrations/tensorflow), [Vision Camera](https://wcandillon.github.io/react-native-webgpu/docs/integrations/vision-camera), and [TypeGPU](https://wcandillon.github.io/react-native-webgpu/docs/integrations/typegpu).

## Installation

React Native WebGPU requires React Native 0.81 or newer. It does not support the legacy architecture.

Please note that the package name is `react-native-wgpu`.

```sh
npm install react-native-wgpu
```

See the [installation guide](https://wcandillon.github.io/react-native-webgpu/docs/installation) for the full requirements.

### With Expo

Expo provides a React Native WebGPU template that works with React Three Fiber, on iOS, Android, and Web:

```sh
npx create-expo-app@latest -e with-webgpu
```

https://github.com/user-attachments/assets/efbd05f8-4ce0-46c2-919c-03e1095bc8ac

## Usage

Usage is identical to the Web. The only React Native specific call is `context.present()`, which presents the frame you have submitted.

```tsx
import { useEffect, useRef } from "react";
import { StyleSheet, View, PixelRatio } from "react-native";
import { Canvas, CanvasRef } from "react-native-wgpu";

export function HelloTriangle() {
  const ref = useRef<CanvasRef>(null);
  useEffect(() => {
    (async () => {
      const adapter = await navigator.gpu.requestAdapter();
      const device = await adapter!.requestDevice();
      const presentationFormat = navigator.gpu.getPreferredCanvasFormat();

      const context = ref.current!.getContext("webgpu")!;
      const canvas = context.canvas as HTMLCanvasElement;
      canvas.width = canvas.clientWidth * PixelRatio.get();
      canvas.height = canvas.clientHeight * PixelRatio.get();

      context.configure({ device, format: presentationFormat, alphaMode: "opaque" });

      // ... encode and submit a render pass ...

      // React Native only: present the frame.
      context.present();
    })();
  }, []);

  return (
    <View style={styles.container}>
      <Canvas ref={ref} style={styles.webgpu} />
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1 },
  webgpu: { flex: 1 },
});
```

The complete example is in the [usage guide](https://wcandillon.github.io/react-native-webgpu/docs/usage).

## Examples

The [example app](/apps/example/) demonstrates every integration documented on the site.

https://github.com/user-attachments/assets/116a41b2-2cf8-49f1-9f16-a5c83637c198

We also provide prebuilt binaries for visionOS and macOS.

https://github.com/user-attachments/assets/2d5c618e-5b15-4cef-8558-d4ddf8c70667

## Contributing

Contributions are welcome! See [CONTRIBUTING.md](/CONTRIBUTING.md) for how to set up the project, build Dawn, and run the test suite.

## License

[MIT](/LICENSE.md)
