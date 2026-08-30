const path = require("path");
const rnwPath = path.dirname(
  require.resolve("react-native-web/package.json", { paths: [__dirname] }),
);
const assetRegistryPath = require.resolve(
  "react-native-web/dist/modules/AssetRegistry/index",
  { paths: [__dirname] },
);

module.exports = function (metroConfig) {
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

    // Let default config handle other modules
    return origResolveRequest(context, moduleName, platform);
  };

  metroConfig.transformer.assetRegistryPath = assetRegistryPath;

  return metroConfig;
};
