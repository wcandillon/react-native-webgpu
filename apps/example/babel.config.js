module.exports = {
  presets: ['module:@react-native/babel-preset'],
  plugins: [
    '@babel/plugin-transform-class-static-block',
    'react-native-reanimated/plugin',
    'transform-inline-environment-variables',
    '@babel/plugin-transform-class-static-block',
    'unplugin-typegpu/babel',
  ],
};
