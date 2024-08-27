const { getDefaultConfig, mergeConfig } = require("@react-native/metro-config");
const path = require("path");

const root = path.resolve(__dirname, "..");
const threePackagePath = path.resolve(root, "node_modules/three");
const threeFiberPackagePath = path.resolve(
  root,
  "./build/react-three-fiber/packages/fiber/src/native",
);

const defaultConfig = getDefaultConfig(__dirname);
defaultConfig.resolver.assetExts.push("glb", "gltf", "jpg", "bin", "hdr");
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
      three: threePackagePath,
      "@react-three/fiber/native": threeFiberPackagePath,
    },
    resolveRequest: (context, moduleName, platform) => {
      if (moduleName === "three/webgpu") {
        return {
          filePath: path.resolve(
            threePackagePath,
            "../../example/build/three.webgpu.js",
          ),
          type: "sourceFile",
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
