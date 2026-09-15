module.exports = {
  presets: ['module:@react-native/babel-preset'],
  plugins: [
    // three.js 0.184 ships static class blocks in its build.
    '@babel/plugin-transform-class-static-block',
    'react-native-reanimated/plugin',
    "transform-inline-environment-variables",
  ],
};
