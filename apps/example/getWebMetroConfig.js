const path = require("path");
const root = path.resolve(__dirname, "../..");
const r3fPath = path.resolve(root, "node_modules/@react-three/fiber");
const rnwPath = path.resolve(root, "node_modules/react-native-web");
const assetRegistryPath = path.resolve(
  root,
  "node_modules/react-native-web/dist/modules/AssetRegistry/index",
);

module.exports = function(metroConfig){
  metroConfig.resolver.platforms = ["ios", "android", "web"];
  const origResolveRequest = metroConfig.resolver.resolveRequest;
  metroConfig.resolver.resolveRequest = (contextRaw, moduleName, platform) => {
    const context = {
      ...contextRaw,
      preferNativePlatform: false,
    };

    if (moduleName === "react-native") {
      return {
        filePath: path.resolve(rnwPath, "dist/index.js"),
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

    // Let default config handle other modules
    return origResolveRequest(context, moduleName, platform);
  };

  metroConfig.transformer.assetRegistryPath = assetRegistryPath;

  return metroConfig
}

