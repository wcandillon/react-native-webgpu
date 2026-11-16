// Shared files must use the Worklets plugin matching the desktop native build.
module.exports = {
  presets: [require.resolve("@react-native/babel-preset")],
  plugins: [
    require.resolve("@babel/plugin-transform-class-static-block"),
    require.resolve("babel-plugin-transform-inline-environment-variables"),
    require.resolve("unplugin-typegpu/babel"),
    require.resolve("react-native-worklets/plugin"),
  ],
};
