import { execSync } from "child_process";
import { writeFileSync } from "fs";
import path from "path";
import { exit } from "process";

const $ = (command: string) => {
  try {
    return execSync(command);
  } catch (e) {
    exit(1);
  }
};

const labels = {
  object: "🧩", // Puzzle piece to represent an object
  descriptor: "📝", // Memo to represent a descriptor
  enum: "🔢", // Input numbers to represent an enumeration
  union: "🔗", // Link symbol to represent a union of types
  errors: "🚨", // Warning sign to represent errors
};

export const writeFile = (
  label: keyof typeof labels,
  name: string,
  content: string,
) => {
  const descriptors = label === "object" ? false : true;
  const file = path.resolve(
    __dirname,
    `../../cpp/rnwgpu/api/${descriptors ? "descriptors" : ""}/${name}.h`,
  );
  $(`touch ${file}`);
  writeFileSync(file, content, "utf8");
  console.log(
    `${labels[label]} ${file.substring(file.indexOf("/package/") + "/package/".length)}`,
  );
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
