import { createRunOncePlugin } from "@expo/config-plugins";
import type { ConfigPlugin } from "@expo/config-plugins";

import { withMetalApiValidationDisabled } from "./withMetalApiValidation";
import { withMinSdkVersion } from "./withMinSdkVersion";

const pkg = require("../../package.json") as {
  name: string;
  version: string;
};

const withWebGPU: ConfigPlugin = (config) =>
  withMinSdkVersion(withMetalApiValidationDisabled(config));

// Expo resolves config plugins through their default export (re-exported by
// app.plugin.js at the package root).
// eslint-disable-next-line import/no-default-export
export default createRunOncePlugin(withWebGPU, pkg.name, pkg.version);
