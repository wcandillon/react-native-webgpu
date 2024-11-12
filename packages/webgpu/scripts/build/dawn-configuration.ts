/* eslint-disable max-len */
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

export const copyHeaders = () => {
  console.log("📗 Copy headers");
  [
    `rm -rf ${projectRoot}/cpp/webgpu`,
    `rm -rf ${projectRoot}/cpp/dawn`,
    `cp -a externals/dawn/out/android_arm64-v8a/gen/include/webgpu ${projectRoot}/cpp`,
    `cp -a externals/dawn/out/android_arm64-v8a/gen/include/dawn ${projectRoot}/cpp`,
    `cp -a externals/dawn/include/webgpu/. ${projectRoot}/cpp/webgpu`,
    `cp -a externals/dawn/include/dawn/. ${projectRoot}/cpp/dawn`,
    `sed -i '' 's/#include "dawn\\/webgpu.h"/#include "webgpu\\/webgpu.h"/' ${projectRoot}/cpp/dawn/dawn_proc_table.h`,
    `cp ${projectRoot}/cpp/dawn/webgpu.h ${projectRoot}/cpp/webgpu/webgpu.h`,
    `cp ${projectRoot}/cpp/dawn/webgpu_cpp.h ${projectRoot}/cpp/webgpu/webgpu_cpp.h`,
    `rm -rf ${projectRoot}/cpp/dawn/webgpu.h`,
    `rm -rf ${projectRoot}/cpp/dawn/webgpu_cpp.h`,
    `rm -rf ${projectRoot}/cpp/dawn/wire`,
    `cp externals/dawn/src/dawn/dawn.json ${projectRoot}/libs`,
  ].map((cmd) => $(cmd));
};

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

export const copyLib = (os: OS, platform: Platform, sdk?: string) => {
  const suffix = `${platform}${sdk ? `_${sdk}` : ""}`;
  const out = `${os}_${suffix}`;
  const dstPath = `${projectRoot}/libs/${os}/${suffix}/`;
  $(`mkdir -p ${dstPath}`);
  if (os === "android") {
    console.log("Strip debug symbols from libwebgpu_dawn.a...");
    $(
      `$ANDROID_NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip externals/dawn/out/${out}/src/dawn/native/libwebgpu_dawn.so`,
    );
  }
  [
    `externals/dawn/out/${out}/src/dawn/native/libwebgpu_dawn.${os === "apple" ? "a" : "so"}`,
  ].forEach((lib) => {
    const libPath = lib;
    console.log(`Copying ${libPath} to ${dstPath}`);
    $(`cp ${libPath} ${dstPath}`);
  });
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
  checkFileExists("libs/dawn.json");
};
