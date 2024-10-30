const { getDefaultConfig } = require("expo/metro-config");
const path = require("path");
const fs = require("fs");

const config = getDefaultConfig(__dirname);

const threePackagePath = path.resolve("../../node_modules/three");

config.watchFolders = [
  path.resolve(__dirname, "../.."),
  path.resolve(__dirname, "../../packages/webgpu"),
];

config.resolver.unstable_enableSymlinks = true

config.resolver.resolveRequest = (context, moduleName, platform) => {
  if (moduleName === "react-native-wgpu") {
    //Not strictly necessary but for monorepo convenience only, use the "react-native" field in the lib package.json to not need to run tsc watch
    context.mainFields = ["react-native"];
  }

  if (moduleName === "DRACOLoader") {
    return {
      filePath: path.resolve(
        threePackagePath,
        "examples/jsm/loaders/DRACOLoader.js"
      ),
      type: "sourceFile",
    };
  }
  if (moduleName === "GLTFLoader") {
    return {
      filePath: path.resolve(
        threePackagePath,
        "examples/jsm/loaders/GLTFLoader.js"
      ),
      type: "sourceFile",
    };
  }
  if (moduleName === "RGBELoader") {
    return {
      filePath: path.resolve(
        threePackagePath,
        "examples/jsm/loaders/RGBELoader.js"
      ),
      type: "sourceFile",
    };
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

config.resolver.assetExts.push("glb", "gltf", "jpg", "bin", "hdr");

config.resolver.extraNodeModules = {
  three: threePackagePath,
};

config.transformer.getTransformOptions = async () => ({
  transform: {
    experimentalImportSupport: false,
    inlineRequires: true,
  },
});

module.exports = config;
