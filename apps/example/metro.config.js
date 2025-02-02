const { makeMetroConfig } = require("@rnx-kit/metro-config");
const path = require('path');

const root = path.resolve(__dirname, '../..');
const threePackagePath = path.resolve(root, 'node_modules/three');

const extraConfig = {
  watchFolders: [root],
  resolver: {
    extraNodeModules: {
      'three': threePackagePath,
    },
    resolveRequest: (context, moduleName, platform) => {
      if (moduleName === "DRACOLoader") {
        return {
          filePath: path.resolve(threePackagePath, 'examples/jsm/loaders/DRACOLoader.js'),
          type: 'sourceFile',
        };
      }
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

      if (moduleName === "BloomNode") {
        return {
          filePath: path.resolve(threePackagePath, 'examples/jsm/tsl/display/BloomNode.js'),
          type: 'sourceFile',
        };
      }
      if (moduleName === 'three' || moduleName === 'three/webgpu') { 
        return {
          filePath: path.resolve(threePackagePath, 'build/three.webgpu.js'),
          type: 'sourceFile',
        };
      }
      if (moduleName === 'three/tsl') { 
        return {
          filePath: path.resolve(threePackagePath, 'build/three.tsl.js'),
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

const metroConfig = makeMetroConfig(extraConfig);
metroConfig.resolver.assetExts.push('glb', 'gltf', 'jpg', 'bin', 'hdr');


module.exports = metroConfig;
