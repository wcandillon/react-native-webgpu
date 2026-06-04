# react-native-wgpu

This package is a thin shim that re-exports [`react-native-webgpu`](https://www.npmjs.com/package/react-native-webgpu) under its previous npm name.

It exists so that projects that depended on the older `react-native-wgpu` name keep working without an immediate code change. New projects should depend on `react-native-webgpu` directly.

## Installation

```
npm install react-native-wgpu
```

This installs `react-native-webgpu` as a dependency. All imports are forwarded:

```ts
import { Canvas } from "react-native-wgpu";
// equivalent to
import { Canvas } from "react-native-webgpu";
```

## Migrating

Replace the dependency in your `package.json`:

```diff
-  "react-native-wgpu": "^0.5.11"
+  "react-native-webgpu": "^0.5.11"
```

and update your imports from `"react-native-wgpu"` to `"react-native-webgpu"`.
