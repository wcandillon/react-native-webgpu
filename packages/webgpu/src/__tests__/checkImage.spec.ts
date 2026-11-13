import fs from "fs";
import os from "os";
import path from "path";

import { PNG } from "pngjs";

import { checkImage } from "./setup";

let mockCI = false;

// A getter written in the object literal would be read once by the object
// spread Babel compiles it to, so it is defined after the spread.
jest.mock("./config", () =>
  Object.defineProperty(
    { ...jest.requireActual("./config"), NODE_WEBGPU: false, REFERENCE: false },
    "CI",
    { enumerable: true, get: () => mockCI },
  ),
);

const image = () => {
  const png = new PNG({ width: 1, height: 1 });
  png.data.fill(255);
  return png;
};

describe("checkImage", () => {
  let directory: string;

  beforeEach(() => {
    directory = fs.mkdtempSync(path.join(os.tmpdir(), "check-image-"));
  });

  afterEach(() => {
    fs.rmSync(directory, { recursive: true, force: true });
  });

  it("fails a check without a reference on CI, keeping its image as a failed check's", () => {
    mockCI = true;
    const reference = path.join(directory, "missing.png");
    expect(() => checkImage(image(), reference)).toThrow(
      "has no reference image",
    );
    expect(fs.existsSync(reference)).toBe(false);
    expect(fs.existsSync(`${reference}.test.png`)).toBe(true);
  });

  it("writes the reference of a check without one outside CI", () => {
    mockCI = false;
    const reference = path.join(directory, "missing.png");
    expect(checkImage(image(), reference)).toBe(0);
    expect(fs.existsSync(reference)).toBe(true);
    expect(fs.existsSync(`${reference}.test.png`)).toBe(false);
  });

  it("compares against an existing reference on CI", () => {
    mockCI = true;
    const reference = path.join(directory, "present.png");
    fs.writeFileSync(reference, PNG.sync.write(image()));
    expect(checkImage(image(), reference)).toBe(0);
    expect(fs.existsSync(`${reference}.test.png`)).toBe(false);
  });
});
