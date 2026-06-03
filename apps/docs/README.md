# React Native WebGPU Documentation

Documentation site for [`react-native-wgpu`](https://www.npmjs.com/package/react-native-wgpu), built with [Docusaurus](https://docusaurus.io/).

The content focuses on:

- Installation requirements
- The differences between React Native WebGPU and the Web
- Integrations with third-party libraries: Three.js, react-three-fiber, TensorFlow.js, Vision Camera, and TypeGPU

## Local development

From the monorepo root:

```bash
yarn
yarn workspace docs start
```

Or from this directory:

```bash
yarn start
```

This starts a local dev server and opens a browser window. Most changes are reflected live without a restart.

## Build

```bash
yarn workspace docs build
```

This generates static content into the `build` directory, which can be served by any static hosting service.

## Type checking

```bash
yarn workspace docs tsc
```
