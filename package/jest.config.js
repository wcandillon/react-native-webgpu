module.exports = {
  preset: 'react-native',
  modulePathIgnorePatterns: [
    '(setup)|(setup.(ts|tsx))$',
    'config.ts',
    'globalSetup.ts',
    'globalTeardown.ts'
  ],
  globalSetup: './src/__tests__/globalSetup.ts',
  globalTeardown: './src/__tests__/globalTeardown.ts'
};