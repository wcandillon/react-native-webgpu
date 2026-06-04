if (typeof __DEV__ !== "undefined" && __DEV__) {
  console.warn(
    "[react-native-wgpu] This package has been renamed to 'react-native-webgpu'. " +
      "The 'react-native-wgpu' shim is deprecated and will be removed in a future release. " +
      "Please install 'react-native-webgpu' and update your imports.",
  );
}

export * from "react-native-webgpu";
