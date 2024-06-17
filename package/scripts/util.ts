import { spawn, execSync } from "child_process";
import { existsSync } from "fs";
import { exit } from "process";

export type OS = "ios" | "android";
export type Platform = "arm64" | "x86_64" | "x86" | "armeabi-v7a" | "arm64-v8a";
export type SDK = "iphoneos" | "iphonesimulator";

export const runAsync = (command: string, label: string): Promise<void> => {
  return new Promise((resolve, reject) => {
    const [cmd, ...args] = command.split(" ");

    const childProcess = spawn(cmd, args, {
      shell: true,
    });

    childProcess.stdout.on("data", (data) => {
      process.stdout.write(`[${label}]${data}`);
    });

    childProcess.stderr.on("data", (data) => {
      process.stderr.write(`[${label} ERROR]: ${data}`);
    });

    childProcess.on("close", (code) => {
      if (code === 0) {
        resolve();
      } else {
        reject(new Error(`[${label}] exited with code ${code}`));
      }
    });

    childProcess.on("error", (error) => {
      reject(new Error(`[${label}] error: ${error.message}`));
    });
  });
};

export const mapKeys = <T extends object>(obj: T) =>
  Object.keys(obj) as (keyof T)[];

export const checkFileExists = (
  filePath: string,
  message: string,
  error: string
) => {
  const exists = existsSync(filePath);
  if (!exists) {
    console.log("");
    console.log("Failed:");
    console.log(message + " not found. (" + filePath + ")");
    console.log(error);
    console.log("");
    exit(1);
  } else {
    console.log("☑ " + message);
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

export const build = async (label: string, args: Record<string, string>) => {
  $(`mkdir -p externals/dawn/out/${label}`);
  process.chdir(`externals/dawn/out/${label}`);
  const cmd = `cmake ../.. -GNinja ${serializeCMakeArgs(args)}`;
  await runAsync(cmd, label);
  await runAsync("ninja", label);
  process.chdir("../../../..");
};

export const copyLib = (os: OS, platform: Platform, sdk?: SDK) => {
  const suffix = `${platform}${sdk ? `_${sdk}` : ""}`;
  const out = `${os}_${suffix}`;
  const libPath = `externals/dawn/out/${out}/src/dawn/native/libwebgpu_dawn.a`;
  const dstPath = `package/libs/${os}/${suffix}/`;
  console.log(`Copying ${libPath} to ${dstPath}`);
  $(`mkdir -p ${dstPath}`);
  $(`cp ${libPath} ${dstPath}`);
};
