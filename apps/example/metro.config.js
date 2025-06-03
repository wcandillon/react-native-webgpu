const { makeMetroConfig } = require("@rnx-kit/metro-config");
const path = require("path");

const root = path.resolve(__dirname, "../..");
const threePackagePath = path.resolve(root, "node_modules/three");
const r3fPath = path.resolve(root, "node_modules/@react-three/fiber");

const rnwPath = path.resolve(root, "node_modules/react-native-web");

const assetRegistryPath = path.resolve(
  root,
  "node_modules/react-native-web/dist/modules/AssetRegistry/index",
);

const IS_WEB = !!process.env.IS_WEB_BUILD;

const extraConfig = {
  watchFolders: [root],
  resolver: {
    extraNodeModules: {
      three: threePackagePath,
    },
    platforms: ["ios", "android", "web"],

    resolveRequest: (contextRaw, moduleName, platform) => {
      const context = IS_WEB
        ? {
            ...contextRaw,
            preferNativePlatform: false,
          }
        : contextRaw;

      if (IS_WEB && moduleName === "react-native") {
        return {
          filePath: path.resolve(rnwPath, "dist/index.js"),
          type: "sourceFile",
        };
      }

      if (moduleName.startsWith("three/addons/")) {
        return {
          filePath: path.resolve(
            threePackagePath,
            "examples/jsm/" + moduleName.replace("three/addons/", "") + ".js",
          ),
          type: "sourceFile",
        };
      }
      if (moduleName === "three" || moduleName === "three/webgpu") {
        return {
          filePath: path.resolve(threePackagePath, "build/three.webgpu.js"),
          type: "sourceFile",
        };
      }
      if (moduleName === "three/tsl") {
        return {
          filePath: path.resolve(threePackagePath, "build/three.tsl.js"),
          type: "sourceFile",
        };
      }

      if (moduleName === "@react-three/fiber") {
        //Do NOT use the stale react three fiber "native" version originally added for expo-gl
        return {
          filePath: path.resolve(r3fPath, "dist/react-three-fiber.esm.js"),
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

const metroConfig = makeMetroConfig(extraConfig);

metroConfig.resolver.assetExts.push("glb", "gltf", "jpg", "bin", "hdr");

if (IS_WEB) {
  metroConfig.transformer.assetRegistryPath = assetRegistryPath;
}

module.exports = metroConfig;
