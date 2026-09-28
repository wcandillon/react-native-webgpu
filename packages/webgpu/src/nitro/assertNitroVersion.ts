import { NitroModules } from "react-native-nitro-modules";

// Oldest react-native-nitro-modules release this package is built and tested
// against. Keep in sync with peerDependencies in package.json.
export const MIN_NITRO_VERSION = "0.37.1";

const parse = (version: string) =>
  version
    .split("-")[0]
    .split(".")
    .map((part) => Number.parseInt(part, 10) || 0);

export const isNitroVersionSupported = (version: string) => {
  const actual = parse(version);
  const min = parse(MIN_NITRO_VERSION);
  for (let i = 0; i < 3; i++) {
    const a = actual[i] ?? 0;
    const m = min[i] ?? 0;
    if (a !== m) {
      return a > m;
    }
  }
  return true;
};

// Throws at startup when the app links an older native Nitro runtime.
export const assertNitroVersion = () => {
  const { version } = NitroModules;
  if (!isNitroVersionSupported(version)) {
    throw new Error(
      `react-native-webgpu requires react-native-nitro-modules >= ${MIN_NITRO_VERSION}, but the app is built with ${version}.`,
    );
  }
};
