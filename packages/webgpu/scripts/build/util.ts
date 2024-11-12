import { spawn, execSync } from "child_process";
import { existsSync } from "fs";
import { exit } from "process";

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
