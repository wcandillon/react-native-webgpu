#!/usr/bin/env tsx

import { execSync } from "child_process";
import { existsSync, mkdirSync, readFileSync, rmSync } from "fs";
import { join } from "path";

import { checkBuildArtifacts } from "./build/dawn-configuration";
import { checkDuplicateHeaders } from "./build/util";

// ANSI color codes
const colors = {
  reset: "\x1b[0m",
  bright: "\x1b[1m",
  dim: "\x1b[2m",
  red: "\x1b[31m",
  green: "\x1b[32m",
  yellow: "\x1b[33m",
  blue: "\x1b[34m",
  cyan: "\x1b[36m",
  white: "\x1b[37m",
};

const symbols = {
  success: "✅",
  error: "❌",
  warning: "⚠️",
  info: "ℹ️",
  download: "⬇️",
  extract: "📦",
  clean: "🧹",
  check: "✓",
  arrow: "→",
  rocket: "🚀",
};

// Helper functions for colored output
const log = {
  info: (msg: string) =>
    console.log(`${colors.cyan}${symbols.info}  ${msg}${colors.reset}`),
  success: (msg: string) =>
    console.log(`${colors.green}${symbols.success} ${msg}${colors.reset}`),
  error: (msg: string) =>
    console.error(`${colors.red}${symbols.error}  ${msg}${colors.reset}`),
  warning: (msg: string) =>
    console.log(`${colors.yellow}${symbols.warning}  ${msg}${colors.reset}`),
  step: (msg: string) =>
    console.log(`${colors.blue}${symbols.arrow} ${msg}${colors.reset}`),
  header: (msg: string) => {
    console.log("");
    console.log(
      `${colors.bright}${colors.cyan}${"=".repeat(60)}${colors.reset}`,
    );
    console.log(
      `${colors.bright}${colors.cyan}${symbols.rocket} ${msg}${colors.reset}`,
    );
    console.log(
      `${colors.bright}${colors.cyan}${"=".repeat(60)}${colors.reset}`,
    );
    console.log("");
  },
  subheader: (msg: string) => {
    console.log("");
    console.log(`${colors.bright}${colors.blue}── ${msg} ──${colors.reset}`);
  },
};

// Read the dawn version from package.json
const packageJsonPath = join(__dirname, "..", "package.json");
const packageJson = JSON.parse(readFileSync(packageJsonPath, "utf-8"));
const dawnVersion = packageJson.dawn;

if (!dawnVersion) {
  log.error("No 'dawn' field found in package.json");
  process.exit(1);
}

// Workarounds in our C++ for Dawn bugs that an upstream fix makes obsolete.
// Each entry names the Dawn release it was validated against. Bumping the pin
// past it fails the install until whoever does the bump checks whether the
// upstream fix is included: if it is, delete the marked code and the entry;
// if it is not, update `pin` here (and re-verify the diagnostic screen).
const dawnWorkarounds = [
  {
    marker: "DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE",
    pin: "chrome-m154",
    files: ["cpp/rnwgpu/SurfaceRegistry.h", "cpp/rnwgpu/api/GPUDevice.cpp"],
    // Android: device.destroy() before the native view is dropped crashed in
    // SwapChain::DetachFromSurfaceImpl (Vulkan FencedDeleter of the dead
    // device). Verify with the "Device Destroy Before Detach" diagnostic in
    // the example app.
    upstream: "Dawn CL pending (see the workaround comment in the files)",
  },
];

for (const workaround of dawnWorkarounds) {
  const root = join(__dirname, "..");
  const present = workaround.files.filter((file) => {
    const path = join(root, file);
    return (
      existsSync(path) &&
      readFileSync(path, "utf-8").includes(workaround.marker)
    );
  });
  if (present.length === 0) {
    log.error(
      `Workaround ${workaround.marker} is gone from the sources; remove its entry from scripts/install-dawn.ts`,
    );
    process.exit(1);
  }
  if (present.length !== workaround.files.length) {
    const missing = workaround.files.filter((file) => !present.includes(file));
    log.error(
      `Workaround ${workaround.marker} is only partially applied: missing from ${missing.join(", ")}`,
    );
    process.exit(1);
  }
  if (dawnVersion !== workaround.pin) {
    log.error(
      `Dawn pin changed (${workaround.pin} -> ${dawnVersion}) but ${workaround.marker} is still in ${present.join(", ")}`,
    );
    log.error(
      `Check whether this Dawn contains the upstream fix (${workaround.upstream}). If it does, remove the marked code and this entry; if not, update the pin in scripts/install-dawn.ts`,
    );
    process.exit(1);
  }
}

// Parse the dawn version to construct the release tag
// Format: "chrome-m152" -> "dawn-chrome-m152"
const releaseTag = `dawn-${dawnVersion.replace("/", "-")}`;
const releaseUrl = `https://github.com/wcandillon/react-native-webgpu/releases/tag/${releaseTag}`;

log.header(`Installing Dawn ${dawnVersion}`);
log.info(`Release: ${colors.dim}${releaseUrl}${colors.reset}`);

// Define the libs directory
const libsDir = join(__dirname, "..", "libs");

// Clean up existing libs directory if it exists
if (existsSync(libsDir)) {
  log.step(`${symbols.clean} Cleaning existing libs directory...`);
  rmSync(libsDir, { recursive: true, force: true });
}

// Create libs directory
mkdirSync(libsDir, { recursive: true });

// Define the cpp directory
const cppDir = join(__dirname, "..", "cpp");

// Define the assets to download
const assets = [
  {
    name: `dawn-android-${releaseTag}.tar.gz`,
    extractTo: libsDir,
    postProcess: () => {
      // Rename dawn-android to android for compatibility
      const oldPath = join(libsDir, "dawn-android");
      const newPath = join(libsDir, "android");
      if (existsSync(oldPath)) {
        execSync(`mv "${oldPath}" "${newPath}"`);
      }
    },
  },
  {
    name: `dawn-apple-${releaseTag}.xcframework.zip`,
    extractTo: libsDir,
    postProcess: () => {
      // The extracted xcframework needs to be placed as libs/apple/libwebgpu_dawn.xcframework
      const extractedPath = join(libsDir, "dawn-apple.xcframework");
      const targetDir = join(libsDir, "apple");
      const targetPath = join(targetDir, "libwebgpu_dawn.xcframework");

      if (existsSync(extractedPath)) {
        // Create apple directory if it doesn't exist
        if (!existsSync(targetDir)) {
          mkdirSync(targetDir, { recursive: true });
        }
        // Move the xcframework to the correct location
        execSync(`mv "${extractedPath}" "${targetPath}"`);
      }
    },
  },
  {
    name: `dawn-headers-${releaseTag}.tar.gz`,
    extractTo: libsDir,
    postProcess: () => {
      // clean folders
      rmSync("cpp/dawn", { recursive: true, force: true });
      rmSync("cpp/webgpu", { recursive: true, force: true });
      // Move headers directly to cpp directory
      const headersIncludePath = join(libsDir, "dawn-headers", "include");
      if (existsSync(join(headersIncludePath, "webgpu"))) {
        execSync(`cp -R "${join(headersIncludePath, "webgpu")}" "${cppDir}/"`);
      }
      if (existsSync(join(headersIncludePath, "dawn"))) {
        execSync(`cp -R "${join(headersIncludePath, "dawn")}" "${cppDir}/"`);
      }
      // Remove the dawn-headers directory after copying
      rmSync(join(libsDir, "dawn-headers"), { recursive: true, force: true });
      rmSync("cpp/dawn/wire", { recursive: true, force: true });
      // Copy headers from cpp/dawn/ to cpp/webgpu/ and then delete source files
      execSync(
        `cp "cpp/dawn/webgpu_cpp_print.h" "cpp/webgpu/webgpu_cpp_print.h"`,
      );
      execSync(`cp "cpp/dawn/webgpu_cpp.h" "cpp/webgpu/webgpu_cpp.h"`);
      execSync(`cp "cpp/dawn/webgpu.h" "cpp/webgpu/webgpu.h"`);
      rmSync("cpp/dawn", { recursive: true, force: true });
      checkDuplicateHeaders(`cpp`);
    },
  },
];

// Download and extract assets
log.subheader("Downloading Assets");

// Add nice names for display
const assetNames: { [key: string]: string } = {
  [`dawn-android-${releaseTag}.tar.gz`]: "Android Libraries",
  [`dawn-apple-${releaseTag}.xcframework.zip`]: "Apple Framework",
  [`dawn-headers-${releaseTag}.tar.gz`]: "C++ Headers",
};

for (const [index, asset] of assets.entries()) {
  const assetUrl = `https://github.com/wcandillon/react-native-webgpu/releases/download/${releaseTag}/${asset.name}`;
  const tarPath = join(libsDir, asset.name);
  const displayName = assetNames[asset.name] || asset.name;

  console.log("");
  log.step(`[${index + 1}/${assets.length}] ${displayName}`);

  try {
    // Download the asset
    process.stdout.write(
      `   ${colors.dim}${symbols.download} Downloading...${colors.reset}`,
    );
    execSync(`curl -L -o "${tarPath}" "${assetUrl}" 2>&1`, { stdio: "pipe" });
    process.stdout.write("\r\x1b[K"); // Clear the line

    // Extract the tar file
    process.stdout.write(
      `   ${colors.dim}${symbols.extract} Extracting...${colors.reset}`,
    );
    if (asset.name.endsWith(".zip")) {
      execSync(`unzip -q -o "${tarPath}" -d "${asset.extractTo}"`, {
        stdio: "pipe",
      });
    } else {
      execSync(`tar -xzf "${tarPath}" -C "${asset.extractTo}"`, {
        stdio: "pipe",
      });
    }
    process.stdout.write("\r\x1b[K"); // Clear the line

    // Remove the tar file after extraction
    rmSync(tarPath);

    // Run post-processing if defined
    if (asset.postProcess) {
      asset.postProcess();
    }

    log.success(`${displayName} installed`);
  } catch (error) {
    console.log(""); // New line after progress
    log.error(`Failed to process ${displayName}`);
    console.error(error);
    process.exit(1);
  }
}

// Verify build artifacts
log.subheader("Verifying Installation");
console.log("");

const originalLog = console.log;
try {
  // Capture the output to format it nicely
  const artifacts: string[] = [];
  console.log = (msg: string) => {
    if (msg.includes("✅")) {
      artifacts.push(msg.replace("✅", "").trim());
    } else if (msg.includes("Check build artifacts")) {
      // Skip this line
    } else if (msg.includes("Failed:")) {
      // Will be handled by the error
    } else {
      originalLog(msg);
    }
  };

  checkBuildArtifacts();
  console.log = originalLog;

  // Display verified artifacts
  artifacts.forEach((artifact) => {
    console.log(
      `   ${colors.green}${symbols.check}${colors.reset} ${colors.dim}${artifact}${colors.reset}`,
    );
  });

  console.log("");
  log.header(`Dawn ${dawnVersion} installed successfully!`);
} catch (error) {
  console.log = originalLog;
  console.log("");
  log.error("Installation verification failed");
  throw error;
}
