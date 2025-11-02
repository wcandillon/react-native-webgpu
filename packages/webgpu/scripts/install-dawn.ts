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

// Parse the dawn version to construct the release tag
// Format: "chromium/7472" -> "dawn-chromium-7472"
const releaseTag = `dawn-${dawnVersion.replace("/", "-")}`;
const releaseUrl = `https://github.com/Shopify/react-native-skia/releases/tag/${releaseTag}`;

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
    name: `dawn-apple-${releaseTag}.xcframework.tar.gz`,
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
  [`dawn-apple-${releaseTag}.xcframework.tar.gz`]: "Apple Framework",
  [`dawn-headers-${releaseTag}.tar.gz`]: "C++ Headers",
};

for (const [index, asset] of assets.entries()) {
  const assetUrl = `https://github.com/Shopify/react-native-skia/releases/download/${releaseTag}/${asset.name}`;
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
    execSync(`tar -xzf "${tarPath}" -C "${asset.extractTo}"`, {
      stdio: "pipe",
    });
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
