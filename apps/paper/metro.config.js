const { getDefaultConfig, mergeConfig } = require('@react-native/metro-config');
const path = require('path');

const root = path.resolve(__dirname, '../..');
const threePackagePath = path.resolve(root, 'node_modules/three');

const defaultConfig = getDefaultConfig(__dirname);
defaultConfig.resolver.assetExts.push('glb', 'gltf', 'jpg', 'bin', 'hdr');
/**
 * Metro configuration
 * https://reactnative.dev/docs/metro
 *
 * @type {import('metro-config').MetroConfig}
 */
const config = {
  watchFolders: [root],

  resolver: {
    extraNodeModules: {
      'three': threePackagePath,
    },
    resolveRequest: (context, moduleName, platform) => {
      if (moduleName === "GLTFLoader") {
        return {
          filePath: path.resolve(threePackagePath, 'examples/jsm/loaders/GLTFLoader.js'),
          type: 'sourceFile',
        };
      }
      if (moduleName === "RGBELoader") {
        return {
          filePath: path.resolve(threePackagePath, 'examples/jsm/loaders/RGBELoader.js'),
          type: 'sourceFile',
        };
      }
      if (moduleName === 'three') { 
        return {
          filePath: path.resolve(threePackagePath, 'build/three.webgpu.js'),
          type: 'sourceFile',
        };
      }
      // Let Metro handle other modules
      return context.resolveRequest(context, moduleName, platform);
    },
  },

  transformer: {
    getTransformOptions: async () => ({
      transform: {
        experimentalImportSupport: false,
        inlineRequires: true,
      },
    }),
  },
};

module.exports = mergeConfig(defaultConfig, config);