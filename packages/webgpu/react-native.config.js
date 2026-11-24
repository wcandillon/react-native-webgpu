// RN 0.87.1 reads this block; RN 0.88+ also reads package.json swiftpmConfig.
// Keep the native product and hook identical in both discovery paths.
// The plugin registers the package/product and the Node-free Xcode phase.
module.exports = {
  spm: {
    name: "ReactNativeWebGPU",
    autolinkingPlugin: "./scripts/spm-plugin.js",
  },
};
