import { withGradleProperties } from "@expo/config-plugins";
import type { ConfigPlugin } from "@expo/config-plugins";

import { setAndroidMinSdkVersion } from "./setAndroidMinSdkVersion";

/**
 * Makes the Android project build against API 26 or newer, which the native
 * code requires. Expo's prebuild template sets `minSdkVersion` from the
 * `android.minSdkVersion` gradle property (24 by default), the same property
 * `expo-build-properties` writes, so raising it in gradle.properties is enough:
 * the library's own build.gradle declares the same floor, which makes the
 * manifest merger reject a lower app minSdkVersion with a clear message
 * instead of the C++ build failing on an unavailable API.
 */
export const withAndroidMinSdkVersion: ConfigPlugin = (config) =>
  withGradleProperties(config, (cfg) => {
    cfg.modResults = setAndroidMinSdkVersion(cfg.modResults);
    return cfg;
  });
