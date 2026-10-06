import { execSync } from "child_process";

import { $, checkFileExists, runAsync } from "./util";

export const libs = ["libwebgpu_dawn"] as const;

export const projectRoot = "packages/webgpu";

export const platforms = [
  "arm64",
  "x86_64",
  "x86",
  "armeabi-v7a",
  "arm64-v8a",
  "universal",
] as const;

export type OS = "apple" | "android";
export type Platform = (typeof platforms)[number];

const serializeCMakeArgs = (args: Record<string, string>) => {
  return Object.keys(args)
    .map((key) => `-D${key}=${args[key]}`)
    .join(" ");
};

export const build = async (
  label: string,
  args: Record<string, string>,
  debugLabel: string,
) => {
  console.log(`🔨 Building ${label}`);
  $(`mkdir -p externals/dawn/out/${label}`);
  process.chdir(`externals/dawn/out/${label}`);
  const cmd = `cmake ../.. -G Ninja ${serializeCMakeArgs(args)}`;
  await runAsync(cmd, debugLabel);
  await runAsync("ninja", debugLabel);
  process.chdir("../../../..");
};

const androidNdkBin = "$ANDROID_NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin";

// Exceptions from the libraries that link Dawn reach JS through Hermes, which
// only matches them against the app's libc++_shared.so.
const assertSharedCxxRuntime = (libPath: string) => {
  const runtimeSymbols = execSync(
    `${androidNdkBin}/llvm-nm -D --defined-only ${libPath}`,
    { maxBuffer: Infinity },
  )
    .toString()
    .split("\n")
    .filter((line) => / (__cxa_throw|__gxx_personality_v0)$/.test(line));
  if (runtimeSymbols.length > 0) {
    throw new Error(
      `${libPath} defines its own C++ runtime; build it with ANDROID_STL=c++_shared`,
    );
  }
};

export const copyLib = (os: OS, platform: Platform, sdk?: string) => {
  const suffix = `${platform}${sdk ? `_${sdk}` : ""}`;
  const out = `${os}_${suffix}`;
  const dstPath = `${projectRoot}/libs/${os}/${suffix}/`;
  const libPath = `externals/dawn/out/${out}/src/dawn/native/libwebgpu_dawn.${os === "android" ? "so" : "a"}`;
  $(`mkdir -p ${dstPath}`);
  if (os === "android") {
    console.log("Strip debug symbols from libwebgpu_dawn.so...");
    $(`${androidNdkBin}/llvm-strip ${libPath}`);
    assertSharedCxxRuntime(libPath);
  }
  console.log(`Copying ${libPath} to ${dstPath}`);
  $(`cp ${libPath} ${dstPath}`);
};

export const checkBuildArtifacts = () => {
  console.log("Check build artifacts...");
  platforms
    .filter((arch) => arch !== "arm64" && arch !== "universal")
    .forEach((platform) => {
      libs.forEach((lib) => {
        checkFileExists(`libs/android/${platform}/${lib}.so`);
      });
    });
  libs.forEach((lib) => {
    checkFileExists(`libs/apple/${lib}.xcframework`);
  });
  //checkFileExists("libs/dawn.json");
};
