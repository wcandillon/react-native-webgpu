import type { AndroidConfig } from "@expo/config-plugins";

type PropertiesItem = AndroidConfig.Properties.PropertiesItem;

const MIN_SDK_VERSION_KEY = "android.minSdkVersion";

/**
 * Raises the app's `android.minSdkVersion` Gradle property, which Expo reads
 * for the app's minSdkVersion (24 when the property is unset), to at least
 * `minimum`. A higher value is kept.
 */
export const setMinSdkVersionAtLeast = (
  properties: PropertiesItem[],
  minimum: number,
): PropertiesItem[] => {
  const index = properties.findIndex(
    (item) => item.type === "property" && item.key === MIN_SDK_VERSION_KEY,
  );
  const current = properties[index];
  if (current?.type === "property" && Number(current.value) >= minimum) {
    return properties;
  }
  const raised: PropertiesItem = {
    type: "property",
    key: MIN_SDK_VERSION_KEY,
    value: String(minimum),
  };
  return index < 0
    ? [...properties, raised]
    : properties.map((item, itemIndex) =>
        itemIndex === index ? raised : item,
      );
};
