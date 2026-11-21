"use strict";

// MARK: Autolinking helpers

const fs = require("fs");
const path = require("path");
const { execFileSync } = require("child_process");

/** Quote a generated path as one shell argument, including spaces/apostrophes. */
const shellQuote = (value) => `'${value.replace(/'/g, "'\\''")}'`;

/**
 * RN requires a JavaScript autolinking entry point. It delegates configuration
 * and package generation to Bash; the installed Xcode phase never invokes Node.
 * RN invokes this hook again when it regenerates the autolinked package graph.
 * Configuration reads the installed release pin; binary downloads wait for Xcode
 * to supply the SDK and architecture build settings.
 */
module.exports = function webgpuSwiftPM(context) {
  const packageRoot = path.join(__dirname, "..");
  const scriptPath = path.join(__dirname, "prepare-apple-binaries.sh");

  // MARK: RN hook compatibility

  // Check the installed hook's capability, rather than assuming a semver string
  // describes it: local RN checkouts and prereleases may carry different tooling.
  // Without scriptPhases, libraries would be linked before they were prepared.
  const contract = path.join(
    context.reactNativeRoot,
    "scripts/spm/autolinking-plugins.js"
  );
  if (
    !fs.existsSync(contract) ||
    !fs.readFileSync(contract, "utf8").includes("scriptPhases")
  ) {
    throw new Error(
      "react-native-webgpu: automatic SwiftPM binary preparation requires React Native 0.87.1 or newer. Upgrade React Native or use CocoaPods."
    );
  }

  // MARK: Generated Dawn link settings

  // WebGPU always supplies Dawn. Skia's integration opts out of its own copy
  // when this native module is linked; both packages validate the same pin.
  // Configuration writes a local link-settings package, without downloading SDKs.
  execFileSync(
    "/bin/bash",
    [
      scriptPath,
      "--config",
      "--spm-package",
      path.join(context.outputDir, "ReactNativeWebGPUBinaries"),
    ],
    { encoding: "utf8" }
  );

  // MARK: Xcode preparation phase

  // RN already resolved the package during autolinking. A project-relative path
  // handles hoisting/pnpm and relocation without Node lookups inside Xcode.
  // Resolve symlinks on both sides before computing the path so workspace package
  // aliases do not produce a different path after Xcode expands SRCROOT.
  const relativeScript = shellQuote(
    path.relative(fs.realpathSync(context.appRoot), fs.realpathSync(scriptPath))
  );

  // Xcode expands SDK/architecture variables when this phase runs. Cache and
  // assembly logic stays in the shared shell scripts used by CocoaPods too.
  // exec preserves their exit status and error diagnostics in Xcode's build log.
  const script = `set -eo pipefail
# Check before reading package files, which script sandboxing may already deny.
if [ "\${ENABLE_USER_SCRIPT_SANDBOXING:-NO}" = YES ]; then
  echo "error: [react-native-webgpu] Set User Script Sandboxing to No on this app target; preparation needs release-cache and build-product access." >&2
  exit 1
fi
PREPARE_SCRIPT="$SRCROOT"/${relativeScript}
if [ ! -f "$PREPARE_SCRIPT" ]; then
  echo "error: [react-native-webgpu] Binary preparation script is missing. Reinstall dependencies and rerun react-native spm update." >&2
  exit 1
fi
exec /bin/bash "$PREPARE_SCRIPT" --output "$BUILT_PRODUCTS_DIR" --assembly-root "$OBJROOT/ReactNativeWebGPUBinaries"
`;

  // MARK: Native package registration

  // RN delegates the plugin host's native contribution entirely to this hook.
  // Recreate its usual libs/<name> alias so Package.swift's relative React and
  // generated-code dependencies resolve within this app's autolinking tree.
  const packageAlias = path.join(
    context.outputDir,
    "libs",
    "ReactNativeWebGPU"
  );
  fs.mkdirSync(path.dirname(packageAlias), { recursive: true });
  if (!fs.existsSync(packageAlias)) {
    fs.symlinkSync(packageRoot, packageAlias, "dir");
  }

  // MARK: RN phase reconciliation and synchronization

  return {
    packageDependencies: [{ name: "ReactNativeWebGPU", path: packageAlias }],
    productDependencies: [
      { name: "ReactNativeWebGPU", package: "ReactNativeWebGPU" },
    ],
    scriptPhases: [
      {
        // RN uses this stable id to update the phase instead of adding duplicates.
        id: "react-native-webgpu.prepare-binaries",
        name: "Prepare React Native WebGPU binaries",
        script,
        // Bundled headers allow package compilation without downloading binaries;
        // prepare libraries before the app's compile/link steps consume them.
        position: "beforeCompile",
        // SDK selection and local overrides are not fixed Xcode input files.
        // Run every build; shell signatures skip unchanged assembly and copies.
        alwaysOutOfDate: true,
      },
    ],
    // These trigger RN autolinking regeneration when the pin or adapter changes.
    // They are separate from the preparation phase's own binary/cache checks.
    watchPaths: [
      path.join(packageRoot, "binaries.json"),
      path.join(packageRoot, "Package.swift"),
      __filename,
      scriptPath,
      path.join(__dirname, "apple-binary-utils.sh"),
    ],
  };
};
