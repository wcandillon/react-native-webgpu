/* eslint-disable max-len */
import { spawn, execSync } from "child_process";
import { existsSync } from "fs";
import { exit } from "process";

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

export const runAsync = (command: string, label: string): Promise<void> => {
  return new Promise((resolve, reject) => {
    const [cmd, ...args] = command.split(" ");
    console.log({ cmd, args });
    const childProcess = spawn(cmd, args, {
      shell: true,
    });

    childProcess.stdout.on("data", (data) => {
      process.stdout.write(`${label} ${data}`);
    });

    childProcess.stderr.on("data", (data) => {
      console.error(`${label} ${data}`);
    });

    childProcess.on("close", (code) => {
      if (code === 0) {
        resolve();
      } else {
        reject(new Error(`${label} exited with code ${code}`));
      }
    });

    childProcess.on("error", (error) => {
      reject(new Error(`${label} ${error.message}`));
    });
  });
};

export const mapKeys = <T extends object>(obj: T) =>
  Object.keys(obj) as (keyof T)[];

export const checkFileExists = (filePath: string) => {
  const exists = existsSync(filePath);
  if (!exists) {
    console.log("");
    console.log("Failed:");
    console.log(filePath + " not found. (" + filePath + ")");
    console.log("");
    exit(1);
  } else {
    console.log("✅ " + filePath);
  }
};

export const $ = (command: string) => {
  try {
    return execSync(command);
  } catch (e) {
    exit(1);
  }
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
