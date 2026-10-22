module.exports = {
  preset: 'react-native',
  modulePathIgnorePatterns: [
    '<rootDir>/lib/',
    '(meshes)|(setup)|(setup.(ts|tsx))',
    'components',
    'config.ts',
    'globalSetup.ts',
    'globalTeardown.ts'
  ],
  // modulePathIgnorePatterns only affects the module registry, not test
  // discovery. Without this, the .d.ts files emitted into lib/ by `bob build`
  // are picked up as test suites and fail with "must contain at least one test".
  // The Expo config plugin has its own standalone jest config
  // (plugin/jest.config.js, run via `yarn test:plugin`), and so do the
  // component tests (jest.unit.config.js, `yarn test:unit`), so neither goes
  // through the e2e globalSetup below.
  testPathIgnorePatterns: [
    '/node_modules/',
    '<rootDir>/lib/',
    '<rootDir>/plugin/',
    '<rootDir>/src/__tests__/unit/',
  ],
  globalSetup: './src/__tests__/globalSetup.ts',
  globalTeardown: './src/__tests__/globalTeardown.ts',
  transform: {
    '^.+\\.(js|jsx|ts|tsx)$': 'babel-jest',
  },
  transformIgnorePatterns: [
    'node_modules/(?!(wgpu-matrix|react-native|@react-native|react-native-.*)/)',
  ],
};