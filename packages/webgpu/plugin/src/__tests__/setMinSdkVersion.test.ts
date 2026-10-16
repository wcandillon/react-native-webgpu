import { setMinSdkVersionAtLeast } from "../setMinSdkVersion";
import { LIBRARY_MIN_SDK_VERSION } from "../withMinSdkVersion";

const property = (key: string, value: string) =>
  ({ type: "property", key, value }) as const;

describe("setMinSdkVersionAtLeast", () => {
  it("adds the property to an app that sets none", () => {
    const result = setMinSdkVersionAtLeast(
      [property("android.useAndroidX", "true")],
      26,
    );
    expect(result).toEqual([
      property("android.useAndroidX", "true"),
      property("android.minSdkVersion", "26"),
    ]);
  });

  it("raises a lower value in place", () => {
    const result = setMinSdkVersionAtLeast(
      [
        { type: "comment", value: "build properties" },
        property("android.minSdkVersion", "24"),
        property("android.useAndroidX", "true"),
      ],
      26,
    );
    expect(result).toEqual([
      { type: "comment", value: "build properties" },
      property("android.minSdkVersion", "26"),
      property("android.useAndroidX", "true"),
    ]);
  });

  it("keeps a higher value", () => {
    const properties = [property("android.minSdkVersion", "28")];
    expect(setMinSdkVersionAtLeast(properties, 26)).toBe(properties);
  });

  it("is idempotent", () => {
    const once = setMinSdkVersionAtLeast([], 26);
    expect(setMinSdkVersionAtLeast(once, 26)).toBe(once);
  });
});

describe("LIBRARY_MIN_SDK_VERSION", () => {
  it("is the floor package.json declares for android/build.gradle", () => {
    expect(Number.isInteger(LIBRARY_MIN_SDK_VERSION)).toBe(true);
  });
});
