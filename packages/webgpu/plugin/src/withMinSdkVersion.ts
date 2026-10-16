import { withGradleProperties } from "@expo/config-plugins";
import type { ConfigPlugin } from "@expo/config-plugins";

import { setMinSdkVersionAtLeast } from "./setMinSdkVersion";

const pkg = require("../../package.json") as {
  androidMinSdkVersion: number;
};

/**
 * The lowest API level the library builds for, which android/build.gradle
 * applies whatever the app's minSdkVersion is.
 */
export const LIBRARY_MIN_SDK_VERSION = pkg.androidMinSdkVersion;

/**
 * Raises an Expo app's minSdkVersion to the library's: Expo's default, 24, is
 * below Android 8 (API 26), whose AHardwareBuffer API the native code calls.
 */
export const withMinSdkVersion: ConfigPlugin = (config) =>
  withGradleProperties(config, (cfg) => {
    cfg.modResults = setMinSdkVersionAtLeast(
      cfg.modResults,
      LIBRARY_MIN_SDK_VERSION,
    );
    return cfg;
  });
