import { spawn, execSync } from "child_process";
import { existsSync } from "fs";
import { exit } from "process";

export const platforms = [
  "arm64",
  "x86_64",
  "x86",
  "armeabi-v7a",
  "arm64-v8a",
] as const;

export type OS = "ios" | "android";
export type Platform = (typeof platforms)[number];
export type SDK = "iphoneos" | "iphonesimulator";

export const runAsync = (command: string, label: string): Promise<void> => {
  console.log(`${command} (${label} in ${process.cwd()})`);
  return new Promise((resolve, reject) => {
    const [cmd, ...args] = command.split(" ");

    const childProcess = spawn(cmd, args, {
      shell: true,
    });

    childProcess.stdout.on("data", (data) => {
      process.stdout.write(`${label} ${data}`);
    });

    childProcess.stderr.on("data", (data) => {
      console.error(`[${label} ERROR]: ${data}`);
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
  dst: string,
  args: Record<string, string>,
  debugLabel: string,
) => {
  $(`mkdir -p package/scripts/build/out/${dst}`);
  process.chdir(`package/scripts/build/out/${dst}`);
  const cmd = `cmake ../.. -GNinja ${serializeCMakeArgs(args)}`;

  await runAsync(cmd, debugLabel);
  await runAsync("ninja", debugLabel);
  process.chdir("../../../../..");
};

export const copyLib = (os: OS, platform: Platform, sdk?: SDK) => {
  const suffix = `${platform}${sdk ? `_${sdk}` : ""}`;
  const out = `${os}_${suffix}`;
  const dstPath = `package/libs/${os}/${suffix}/`;
  $(`mkdir -p ${dstPath}`);
  [
    `package/scripts/build/out/${out}/libwebgpu_c_bundled.${os === "ios" ? "a" : "a"}`,
  ].forEach((lib) => {
    const libPath = lib;
    console.log(`Copying ${libPath} to ${dstPath}`);
    $(`cp ${libPath} ${dstPath}`);
  });
};

export const checkBuildArtifacts = () => {
  console.log("Check build artifacts...");
  // platforms
  //   .filter((arch) => arch !== "arm64")
  //   .forEach((platform) => {
  //     libs.forEach((lib) => {
  //       checkFileExists(`libs/android/${platform}/${lib}.a`);
  //     });
  //   });
  // libs.forEach((lib) => {
  //   checkFileExists(`libs/ios/${lib}.xcframework`);
  // });
  // checkFileExists("cpp/dawn/webgpu_cpp.h");
};
