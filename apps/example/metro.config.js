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
      if (moduleName === 'three/webgpu') { 
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

const metroConfig = makeMetroConfig(extraConfig);
metroConfig.resolver.assetExts.push('glb', 'gltf', 'jpg', 'bin', 'hdr');


module.exports = metroConfig;
