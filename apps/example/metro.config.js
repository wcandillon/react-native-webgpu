const { getDefaultConfig, mergeConfig } = require("@react-native/metro-config");
const path = require('path');
const getWebMetroConfig = require('./getWebMetroConfig');

const root = path.resolve(__dirname, '../..');
const threePackagePath = path.resolve(root, 'node_modules/three');

const defaultConfig = getDefaultConfig(__dirname);

const customConfig = {
  watchFolders: [root],
  resolver: {
    ...defaultConfig.resolver,
    extraNodeModules: {
      'three': threePackagePath,
    },
    resolveRequest: (context, moduleName, platform) => {
      if (moduleName.startsWith('three/addons/')) {
        return {
          filePath: path.resolve(threePackagePath, 'examples/jsm/' + moduleName.replace('three/addons/', '') + '.js'),
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

      if (moduleName === "@react-three/fiber") {
        //Just use the vanilla web build of react three fiber, not the stale "native" code path which has not been kept up to date.
        return {
          filePath: path.resolve(r3fPath, "dist/react-three-fiber.esm.js"),
          type: "sourceFile",
        };
      }
      // Let Metro handle other modules
      return context.resolveRequest(context, moduleName, platform);
    },
    assetExts: [...defaultConfig.resolver.assetExts, 'glb', 'gltf', 'jpg', 'bin', 'hdr'],
  },

  transformer: {
    ...defaultConfig.transformer,
    getTransformOptions: async () => ({
      transform: {
        experimentalImportSupport: false,
        inlineRequires: true,
      },
    }),
  },
};

const metroConfig = mergeConfig(defaultConfig, customConfig);

module.exports = !!process.env.IS_WEB_BUILD ? getWebMetroConfig(metroConfig) : metroConfig;
