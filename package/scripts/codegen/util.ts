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
