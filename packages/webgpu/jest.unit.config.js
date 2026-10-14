// Component tests rendered under Jest with React Native's mocks and
// React Native Testing Library. The package's main jest config drives the e2e
// harness, which waits for the example app; these need no device.
module.exports = {
  preset: 'react-native',
  rootDir: __dirname,
  testMatch: ['<rootDir>/src/__tests__/unit/**/*.test.ts?(x)'],
  transform: {
    '^.+\\.(js|jsx|ts|tsx)$': 'babel-jest',
  },
  transformIgnorePatterns: [
    'node_modules/(?!(wgpu-matrix|react-native|@react-native|react-native-.*)/)',
  ],
};
