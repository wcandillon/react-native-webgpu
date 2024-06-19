const path = require("path");

const { getDefaultConfig, mergeConfig } = require("@react-native/metro-config");
const escape = require("escape-string-regexp");
const exclusionList = require("metro-config/src/defaults/exclusionList");

const pak = require("./package.json");

const modules = Object.keys({ ...pak.peerDependencies });

const glob = require("glob-to-regexp");

function getBlacklist() {
  const nodeModuleDirs = [
    glob(`${path.resolve(__dirname, "../package")}/node_modules/*`),
  ];
  return exclusionList(nodeModuleDirs);
}

/**
 * Metro configuration
 * https://facebook.github.io/metro/docs/configuration
 *
 * @type {import('metro-config').MetroConfig}
 */
const config = {
  watchFolders: [path.resolve("."), path.resolve("../package")],

  resolver: {
    blacklistRE: getBlacklist(),
    extraNodeModules: modules.reduce((acc, name) => {
      acc[name] = path.join(__dirname, "node_modules", name);
      return acc;
    }, {}),
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

module.exports = mergeConfig(getDefaultConfig(__dirname), config);
