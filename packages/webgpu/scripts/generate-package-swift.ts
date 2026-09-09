#!/usr/bin/env tsx

// Regenerates packages/webgpu/Package.swift for the Dawn release pinned in
// package.json ("dawn" field). Run this after build-dawn.yml has published a
// dawn-apple-<tag>.xcframework.zip release asset. Writes the file locally;
// commit and push it yourself.
//
// Pass --local to point the WebGPUDawn binary target at the xcframework
// already sitting in libs/apple/ (from `yarn install-dawn` or a local Dawn
// build) instead of downloading the release — useful for iterating on the
// package/example locally without a network round trip.

import { execSync } from "child_process";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "fs";
import { tmpdir } from "os";
import { join } from "path";

import { renderPackageSwift, type WebGPUDawnBinaryTarget } from "./package-swift-template";

const packageJsonPath = join(__dirname, "..", "package.json");
const packageJson = JSON.parse(readFileSync(packageJsonPath, "utf-8"));
const dawnVersion = packageJson.dawn;

if (!dawnVersion) {
  console.error("No 'dawn' field found in package.json");
  process.exit(1);
}

const releaseTag = `dawn-${dawnVersion.replace("/", "-")}`;
const zipName = `dawn-apple-${releaseTag}.xcframework.zip`;
const zipUrl = `https://github.com/wcandillon/react-native-webgpu/releases/download/${releaseTag}/${zipName}`;
const checksumUrl = `${zipUrl}.checksum.txt`;

const fetchChecksum = (): string => {
  // Prefer the checksum file build-dawn.yml computes and uploads alongside
  // the zip — avoids re-downloading the (large) xcframework locally.
  try {
    const checksum = execSync(`curl -sfL "${checksumUrl}"`, {
      encoding: "utf-8",
    }).trim();
    if (checksum) {
      console.log(`Using published checksum from ${checksumUrl}`);
      return checksum;
    }
  } catch {
    console.log(
      "Published checksum file not found, computing from the zip instead...",
    );
  }

  const tmpDir = mkdtempSync(join(tmpdir(), "rnwgpu-package-swift-"));
  const zipPath = join(tmpDir, zipName);
  try {
    execSync(`curl -fL -o "${zipPath}" "${zipUrl}"`, { stdio: "inherit" });
    return execSync(`swift package compute-checksum "${zipPath}"`, {
      encoding: "utf-8",
    }).trim();
  } finally {
    rmSync(tmpDir, { recursive: true, force: true });
  }
};

const useLocal = process.argv.includes("--local");

console.log(
  useLocal
    ? "Generating Package.swift against the local xcframework in libs/apple/..."
    : `Generating Package.swift for ${releaseTag}...`,
);

const target: WebGPUDawnBinaryTarget = useLocal
  ? { kind: "local", path: "libs/apple/libwebgpu_dawn.xcframework" }
  : { kind: "remote", url: zipUrl, checksum: fetchChecksum() };

const outputPath = join(__dirname, "..", "Package.swift");
writeFileSync(outputPath, renderPackageSwift(target));

console.log(`Wrote ${outputPath}`);
if (target.kind === "remote") {
  console.log(`  url:      ${target.url}`);
  console.log(`  checksum: ${target.checksum}`);
} else {
  console.log(`  path: ${target.path}`);
}
