import * as fs from "fs";
import * as path from "path";

import { withDangerousMod } from "@expo/config-plugins";
import type { ConfigPlugin } from "@expo/config-plugins";

import { setMetalApiValidationDisabled } from "./setMetalApiValidation";

const getSharedSchemes = (platformProjectRoot: string): string[] => {
  if (!fs.existsSync(platformProjectRoot)) {
    return [];
  }
  return fs
    .readdirSync(platformProjectRoot)
    .filter((entry) => entry.endsWith(".xcodeproj"))
    .map((entry) =>
      path.join(platformProjectRoot, entry, "xcshareddata", "xcschemes"),
    )
    .filter((schemesDir) => fs.existsSync(schemesDir))
    .flatMap((schemesDir) =>
      fs
        .readdirSync(schemesDir)
        .filter((file) => file.endsWith(".xcscheme"))
        .map((file) => path.join(schemesDir, file)),
    );
};

/**
 * Disables Metal API validation in every shared Xcode scheme of the iOS
 * project. Dawn (the WebGPU implementation) triggers false-positive
 * validation errors on the iOS Simulator that do not occur on device.
 * Manually unchecking the setting in Xcode does not survive
 * `npx expo prebuild --clean`, which is why this is a config plugin.
 */
export const withMetalApiValidationDisabled: ConfigPlugin = (config) =>
  withDangerousMod(config, [
    "ios",
    async (cfg) => {
      const schemes = getSharedSchemes(cfg.modRequest.platformProjectRoot);
      for (const scheme of schemes) {
        const contents = fs.readFileSync(scheme, "utf8");
        fs.writeFileSync(scheme, setMetalApiValidationDisabled(contents));
      }
      return cfg;
    },
  ]);
