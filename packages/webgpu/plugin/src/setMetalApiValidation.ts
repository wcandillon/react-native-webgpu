/**
 * Rewrites an Xcode scheme (.xcscheme XML) so that Metal API validation is
 * disabled for the Run action — the same effect as unchecking
 * Product → Scheme → Edit Scheme → Run → Diagnostics → Metal API Validation.
 *
 * Xcode encodes the setting as the `enableGPUValidationMode` attribute on the
 * <LaunchAction> element: the attribute is absent when validation is enabled
 * (the default) and set to "1" when disabled. This mirrors how Flutter
 * disables Metal API validation in its templates
 * (https://github.com/flutter/flutter/pull/159228).
 */
export const setMetalApiValidationDisabled = (xcscheme: string): string =>
  xcscheme.replace(
    /<LaunchAction([^>]*)>/,
    (launchAction, attributes: string) => {
      if (attributes.includes("enableGPUValidationMode")) {
        return launchAction.replace(
          /enableGPUValidationMode\s*=\s*"[^"]*"/,
          'enableGPUValidationMode = "1"',
        );
      }
      return `<LaunchAction${attributes}\n      enableGPUValidationMode = "1">`;
    },
  );
