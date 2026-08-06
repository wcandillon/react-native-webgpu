import fs from "fs";
import path from "path";

import { client } from "./setup";

const cppDir = path.resolve(__dirname, "..", "..", "cpp");
const read = (...segments: string[]) =>
  fs.readFileSync(path.join(cppDir, ...segments), "utf-8");

// Every value of wgpu::FeatureName, straight from the Dawn headers installed by
// `yarn install-dawn`. A milestone upgrade grows this list.
const dawnFeatureNames = () => {
  const header = read("webgpu", "webgpu_cpp.h");
  const body = /enum class FeatureName[^{]*\{([\s\S]*?)\n\};/.exec(header);
  if (!body) {
    throw new Error("Could not find enum class FeatureName in webgpu_cpp.h");
  }
  return Array.from(body[1].matchAll(/^\s*(\w+)\s*=/gm))
    .map((match) => match[1])
    .filter((name) => name !== "Undefined");
};

// The single source of truth every conversion is generated from.
const mappingList = () => {
  const header = read("rnwgpu", "api", "GPUFeatures.h");
  const body = /#define RNWGPU_FOR_EACH_FEATURE_NAME\(V\)([\s\S]*?)\n\n/.exec(
    header,
  );
  if (!body) {
    throw new Error("Could not find RNWGPU_FOR_EACH_FEATURE_NAME");
  }
  // `[\s\\]*` so an entry clang-format wrapped across a line continuation is
  // still picked up.
  return Array.from(body[1].matchAll(/V\((\w+),[\s\\]*"([^"]+)"\)/g)).map(
    (match) => ({ enumerator: match[1], name: match[2] }),
  );
};

describe("GPUFeatureName mapping", () => {
  it("maps every wgpu::FeatureName", () => {
    const mapped = new Set(mappingList().map(({ enumerator }) => enumerator));
    const missing = dawnFeatureNames().filter((name) => !mapped.has(name));
    // A Dawn upgrade added features we do not convert yet: adapter.features
    // would report them as "" and requiredFeatures would reject them. Add them
    // to RNWGPU_FOR_EACH_FEATURE_NAME in cpp/rnwgpu/api/GPUFeatures.h.
    expect(missing).toEqual([]);
  });

  it("maps no feature that Dawn does not define", () => {
    const known = new Set(dawnFeatureNames());
    const stale = mappingList()
      .map(({ enumerator }) => enumerator)
      .filter((enumerator) => !known.has(enumerator));
    expect(stale).toEqual([]);
  });

  it("gives every feature a unique kebab-case name", () => {
    const list = mappingList();
    expect(list.length).toBeGreaterThan(0);
    const duplicates = list
      .map(({ name }) => name)
      .filter((name, index, names) => names.indexOf(name) !== index);
    expect(duplicates).toEqual([]);
    const malformed = list
      .map(({ name }) => name)
      .filter((name) => !/^[a-z0-9]+(-[a-z0-9]+)*$/.test(name));
    expect(malformed).toEqual([]);
  });

  it("derives both EnumMapper conversions from the shared list", () => {
    // Guards against someone re-introducing a hand-written table that drifts
    // from GPUFeatures.h, which is how these mappings fell behind before.
    const unions = read("rnwgpu", "api", "descriptors", "Unions.h");
    const featureBlocks = unions
      .split(/\ninline void convert/)
      .filter((block) => /wgpu::FeatureName [*]?(out|in)Enum/.test(block));
    expect(featureBlocks).toHaveLength(2);
    featureBlocks.forEach((block) => {
      expect(block).toContain("RNWGPU_FOR_EACH_FEATURE_NAME");
    });
  });

  it("never reports an unmapped feature on the adapter", async () => {
    const features = await client.eval(({ gpu }) =>
      gpu.requestAdapter().then((adapter) => Array.from(adapter!.features)),
    );
    expect(features.length).toBeGreaterThan(0);
    // "" is what convertEnumToJSUnion falls back to for a feature it does not
    // know about.
    expect(features.filter((feature) => feature === "")).toEqual([]);
  });

  it("never reports an unmapped feature on the device", async () => {
    const features = await client.eval(({ device }) =>
      Array.from(device.features),
    );
    expect(features.filter((feature) => feature === "")).toEqual([]);
  });

  it("accepts every advertised feature in requiredFeatures", async () => {
    // Round-trips the whole table through native: JS name -> wgpu::FeatureName
    // on the way in, and back to a JS name when the device reports what it
    // enabled. Requesting any subset of adapter.features must succeed.
    const result = await client.eval(({ gpu }) =>
      gpu.requestAdapter().then((adapter) => {
        const requested = Array.from(adapter!.features);
        return adapter!
          .requestDevice({
            requiredFeatures: requested as GPUFeatureName[],
          })
          .then((device) => ({
            requested,
            enabled: Array.from(device.features),
          }));
      }),
    );
    expect(result.requested.length).toBeGreaterThan(0);
    const missing = result.requested.filter(
      (feature) => !result.enabled.includes(feature),
    );
    expect(missing).toEqual([]);
  });
});
