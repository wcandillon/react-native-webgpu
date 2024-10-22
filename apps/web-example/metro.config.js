const { getDefaultConfig } = require("expo/metro-config");
const path = require("path");

const config = getDefaultConfig(__dirname);

config.watchFolders = [
  path.resolve(__dirname, "../.."),
  path.resolve(__dirname, "../../packages/webgpu"),
];

config.resolver.resolveRequest = (context, moduleName, platform) => {
  if (moduleName === "react-native-wgpu") {
    //For monorepo convenience only, use the "react-native" field in package.json
    context.mainFields = ["react-native"];
  }

  try {
    //Force monorepo dependencies to first use THIS node_modules when resolving their packages.
    return context.resolveRequest(
      {
        ...context,
        originModulePath: __dirname + "/shim.js",
      },
      moduleName,
      platform
    );
  } catch (e) {
    //Do nothing. Just trying this node_modules first
  }

  return context.resolveRequest(context, moduleName, platform);
};

module.exports = config;
