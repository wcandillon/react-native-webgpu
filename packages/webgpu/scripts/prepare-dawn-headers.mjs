/**
 * Prepares source/generated Dawn headers for contributors and npm publishing.
 * Reads only binaries.json and shares Skia's OS-specific release cache.
 * Requires Node plus unzip (macOS/Linux) or PowerShell (Windows); no Yarn install.
 * Apple Xcode preparation is separate and never invokes this script.
 */
import crypto from "node:crypto";
import { execFileSync } from "node:child_process";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";

const PackageRoot = path.resolve(
  path.dirname(fileURLToPath(import.meta.url)),
  ".."
);
const AssetName = "dawn-headers.zip";
const ReleaseBaseURL =
  "https://github.com/wcandillon/react-native-skia/releases/download";

/** Hash the small header archive before extracting or reusing it.
 * @param {string} file
 */
const sha256 = (file) =>
  crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex");

/** Match Skia's user-cache location and respect the shared override. */
const cacheDirectory = () => {
  if (process.env.RNSKIA_CACHE_DIR) {
    if (!path.isAbsolute(process.env.RNSKIA_CACHE_DIR)) {
      throw new Error("RNSKIA_CACHE_DIR must be an absolute path.");
    }
    return process.env.RNSKIA_CACHE_DIR;
  }
  let root = process.env.XDG_CACHE_HOME ?? path.join(os.homedir(), ".cache");
  if (process.platform === "darwin") {
    root = path.join(os.homedir(), "Library", "Caches");
  } else if (process.platform === "win32") {
    root =
      process.env.LOCALAPPDATA ?? path.join(os.homedir(), "AppData", "Local");
  }
  return path.join(root, "react-native-skia");
};

/** Fetch one pinned asset; mirrors may also use file:/// URLs.
 * @param {URL} url
 * @param {string} destination
 */
const download = async (url, destination) => {
  if (url.protocol === "file:") {
    fs.copyFileSync(fileURLToPath(url), destination);
    return;
  }
  const response = await fetch(url, { signal: AbortSignal.timeout(60_000) });
  if (!response.ok) {
    throw new Error(`Cannot download ${url}: HTTP ${response.status}.`);
  }
  fs.writeFileSync(destination, Buffer.from(await response.arrayBuffer()));
};

/** Use system ZIP extraction so the SDK syntax job needs no npm dependencies.
 * @param {string} archive
 * @param {string} destination
 */
const extract = (archive, destination) => {
  if (process.platform === "win32") {
    // Environment values preserve spaces/apostrophes without PowerShell quoting.
    execFileSync(
      "powershell.exe",
      [
        "-NoProfile",
        "-NonInteractive",
        "-Command",
        "Expand-Archive -LiteralPath $env:RNWGPU_HEADERS_ARCHIVE -DestinationPath $env:RNWGPU_HEADERS_DESTINATION -ErrorAction Stop",
      ],
      {
        env: {
          ...process.env,
          RNWGPU_HEADERS_ARCHIVE: archive,
          RNWGPU_HEADERS_DESTINATION: destination,
        },
      }
    );
    return;
  }
  execFileSync("unzip", ["-q", archive, "-d", destination]);
};

/** Verify the release archive before replacing cpp/dawn/include. */
const prepare = async () => {
  const binaries = JSON.parse(
    fs.readFileSync(path.join(PackageRoot, "binaries.json"), "utf8")
  );
  const { version, tag } = binaries;
  const expected = binaries.assets?.[AssetName]?.sha256;
  if (
    !/^\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?$/.test(version) ||
    tag !== `binaries-${version}` ||
    !/^[a-f0-9]{64}$/.test(expected)
  ) {
    throw new Error(
      "Invalid release or Dawn header checksum in binaries.json."
    );
  }

  const cache = path.join(cacheDirectory(), tag, expected);
  const archive = path.join(cache, AssetName);
  fs.mkdirSync(cache, { recursive: true });
  if (!fs.existsSync(archive) || sha256(archive) !== expected) {
    const url = new URL(
      `${tag}/${AssetName}`,
      `${(process.env.REACT_NATIVE_SKIA_BINARIES_URL ?? ReleaseBaseURL).replace(
        /\/$/,
        ""
      )}/`
    );
    // Publish complete verified ZIPs atomically; other projects share this cache.
    const temporary = fs.mkdtempSync(path.join(cache, ".headers-"));
    try {
      const candidate = path.join(temporary, AssetName);
      console.log(`Downloading ${url}`);
      await download(url, candidate);
      if (sha256(candidate) !== expected) {
        throw new Error(
          `SHA-256 mismatch for ${AssetName}; download discarded.`
        );
      }
      fs.renameSync(candidate, archive);
    } finally {
      fs.rmSync(temporary, { recursive: true, force: true });
    }
  } else {
    console.log(`Verified cached ${AssetName} (${version}).`);
  }

  const dawnRoot = path.join(PackageRoot, "cpp", "dawn");
  fs.mkdirSync(dawnRoot, { recursive: true });
  const temporary = fs.mkdtempSync(path.join(dawnRoot, ".headers-"));
  const destination = path.join(dawnRoot, "include");
  const previous = path.join(temporary, "previous");
  try {
    extract(archive, temporary);
    const include = path.join(temporary, "include");
    if (!fs.existsSync(path.join(include, "webgpu", "webgpu.h"))) {
      throw new Error(`${AssetName} has no include/webgpu/webgpu.h.`);
    }
    // Keep existing headers if download/extraction fails. Leave vendored common/
    // untouched: only the generated/source include tree is replaced.
    if (fs.existsSync(destination)) {
      fs.renameSync(destination, previous);
    }
    try {
      fs.renameSync(include, destination);
    } catch (error) {
      if (fs.existsSync(previous)) {
        fs.renameSync(previous, destination);
      }
      throw error;
    }
    console.log(`Prepared Dawn headers ${version}: ${destination}`);
  } finally {
    fs.rmSync(temporary, { recursive: true, force: true });
  }
};

prepare().catch((error) => {
  console.error(
    `Dawn header preparation failed: ${error.message}\nCheck binaries.json, the published release, and REACT_NATIVE_SKIA_BINARIES_URL, then retry.`
  );
  process.exitCode = 1;
});
