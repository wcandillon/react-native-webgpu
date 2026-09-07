module.exports = {
  expo: {
    name: "Expo WebGPU",
    slug: "expo-webgpu",
    ios: {
      bundleIdentifier: "com.anonymous.expowebgpu",
    },
    android: {
      package: "com.anonymous.expowebgpu",
    },
    plugins: ["expo-router", "react-native-webgpu"],
  },
};
