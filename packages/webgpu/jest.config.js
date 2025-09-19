module.exports = {
  preset: 'react-native',
  modulePathIgnorePatterns: [
    '(meshes)|(setup)|(setup.(ts|tsx))',
    'components',
    'config.ts',
    'globalSetup.ts',
    'globalTeardown.ts'
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