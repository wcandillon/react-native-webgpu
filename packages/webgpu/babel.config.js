const plugins = [];

try {
  require.resolve('react-native-worklets/plugin');
  plugins.push('react-native-worklets/plugin');
} catch {
  // react-native-worklets is optional
}

module.exports = {
  presets: ['module:@react-native/babel-preset'],
  plugins,
};