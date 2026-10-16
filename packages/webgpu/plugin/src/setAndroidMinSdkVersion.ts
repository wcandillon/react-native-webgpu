import type { AndroidConfig } from "@expo/config-plugins";

type PropertiesItem = AndroidConfig.Properties.PropertiesItem;

const pkg = require("../../package.json") as {
  androidMinSdkVersion: number;
};

/**
 * The lowest Android API level the native code compiles against: Android 8.0
 * (API 26) introduced AHardwareBuffer, which shared texture memory, external
 * textures and view snapshots are built on. Declared once, as
 * `androidMinSdkVersion` in package.json, which android/build.gradle reads too.
 */
export const REQUIRED_MIN_SDK_VERSION: number = pkg.androidMinSdkVersion;

/**
 * The gradle property Expo's android/build.gradle template reads its
 * `minSdkVersion` from (`findProperty('android.minSdkVersion')`); it is also
 * what `expo-build-properties` writes.
 */
export const MIN_SDK_VERSION_PROPERTY = "android.minSdkVersion";

/**
 * Raises `android.minSdkVersion` in the parsed gradle.properties to the API
 * level the library needs, leaving it alone when the app already asks for a
 * higher one. Returns the same list when nothing has to change.
 */
export const setAndroidMinSdkVersion = (
  properties: PropertiesItem[],
  minSdkVersion: number = REQUIRED_MIN_SDK_VERSION,
): PropertiesItem[] => {
  const value = String(minSdkVersion);
  const existing = properties.find(
    (item) => item.type === "property" && item.key === MIN_SDK_VERSION_PROPERTY,
  );
  if (!existing) {
    return [
      ...properties,
      { type: "property", key: MIN_SDK_VERSION_PROPERTY, value },
    ];
  }
  if (existing.type !== "property") {
    return properties;
  }
  const current = Number.parseInt(existing.value, 10);
  if (Number.isFinite(current) && current >= minSdkVersion) {
    return properties;
  }
  return properties.map((item) =>
    item === existing ? { ...item, value } : item,
  );
};
