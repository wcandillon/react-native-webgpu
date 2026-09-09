const path = require('path');
const { getDefaultConfig, mergeConfig } = require('@react-native/metro-config');

// Workspace deps (react-native-webgpu, @babel/runtime, etc.) get hoisted to
// the monorepo root node_modules, so Metro needs to watch it too.
const root = path.resolve(__dirname, '../..');

/**
 * Metro configuration
 * https://reactnative.dev/docs/metro
 *
 * @type {import('@react-native/metro-config').MetroConfig}
 */
const config = {
  watchFolders: [root],
};

module.exports = mergeConfig(getDefaultConfig(__dirname), config);
