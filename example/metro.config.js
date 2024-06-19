const path = require("path");

const { getDefaultConfig, mergeConfig } = require("@react-native/metro-config");
const exclusionList = require("metro-config/src/defaults/exclusionList");
const glob = require("glob-to-regexp");

const defaultConfig = getDefaultConfig(__dirname);

function getBlacklist() {
  const nodeModuleDirs = [
    glob(`${path.resolve(__dirname, "../package")}/node_modules/*`),
  ];
  return exclusionList(nodeModuleDirs);
}

const config = {
  // workaround for an issue with symlinks encountered starting with
  // metro@0.55 / React Native 0.61
  // (not needed with React Native 0.60 / metro@0.54)
  resolver: {
    extraNodeModules: new Proxy(
      {},
      { get: (_, name) => path.resolve(".", "node_modules", name) }
    ),
    blacklistRE: getBlacklist(),
  },

  // quick workaround for another issue with symlinks
  watchFolders: [path.resolve("."), path.resolve("../package")],

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
