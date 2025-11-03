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

export const checkDuplicateHeaders = (cppPath: string) => {
  // Check for duplicate header names and issue warnings
  const duplicateHeaders = $(
    `find ${cppPath} -name '*.h' -type f | sed 's/.*\\///' | sort | uniq -d`,
  ).toString();
  if (duplicateHeaders.trim()) {
    console.warn("⚠️  WARNING: Found duplicate header names:");
    let hasConflicts = false;

    duplicateHeaders
      .split("\n")
      .filter(Boolean)
      .forEach((filename: string) => {
        const fullPaths = $(
          `find ${cppPath} -name "${filename}" -type f`,
        ).toString();
        const paths = fullPaths.split("\n").filter(Boolean);

        console.warn(`   ${filename}:`);
        paths.forEach((filePath: string) => {
          console.warn(`     ${filePath}`);
        });

        hasConflicts = true;
      });

    if (hasConflicts) {
      console.error(
        "❌ ERROR: Duplicate headers found that will cause iOS build conflicts!",
      );
      exit(1);
    }
  }
};
