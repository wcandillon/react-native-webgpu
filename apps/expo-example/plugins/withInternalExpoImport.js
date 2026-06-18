const { withAppDelegate } = require("expo/config-plugins");

// SDK 56 generates `internal import Expo` in ExpoModulesProvider.swift, but the
// AppDelegate.swift template uses a plain `import Expo`. Under Swift 6 that is
// "ambiguous implicit access level for import of 'Expo'". We make it explicit
// with `public import` (not `internal`): any explicit level resolves the
// ambiguity, and `public` keeps ExpoAppDelegate public so `public class
// AppDelegate: ExpoAppDelegate` stays valid. Runs on every prebuild.
module.exports = function withInternalExpoImport(config) {
  return withAppDelegate(config, (cfg) => {
    if (cfg.modResults.language === "swift") {
      cfg.modResults.contents = cfg.modResults.contents.replace(
        /^import Expo$/m,
        "public import Expo"
      );
    }
    return cfg;
  });
};
