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

export const writeFile = (
  name: string,
  content: string,
  descriptors = true,
) => {
  const file = path.resolve(
    __dirname,
    `../../cpp/rnwgpu/api/${descriptors ? "descriptors" : ""}/${name}.h`,
  );
  $(`touch ${file}`);
  writeFileSync(file, content, "utf8");
  console.log(
    `Generated ${file.substring(file.indexOf("/package/") + "/package/".length)}`,
  );
};
