import { client } from "./setup";

describe("Adapter", () => {
  it("executes a simple function", async () => {
    const result = await client.eval(() => 1 + 1);
    expect(result).toBe(2);
  });
  it("executes a simple function from context", async () => {
    const result = await client.eval(({ a, b, c }) => a + b + c, {
      a: 1,
      b: 2,
      c: 3,
    });
    expect(result).toBe(6);
  });
  it("execute a simple async function (1)", async () => {
    const result = await client.eval(
      () =>
        new Promise<number>((resolve) => setTimeout(() => resolve(1 + 1), 100)),
    );
    expect(result).toBe(2);
  });
  it("requestAdapter (1)", async () => {
    const result = await client.eval(({ gpu }) => {
      return gpu.requestAdapter().then((adapter) => adapter != null);
    });
    expect(result).toBe(true);
  });
  it("requestAdapter (2)", async () => {
    const result = await client.eval(({ gpu }) => {
      return gpu.requestAdapter(undefined).then((adapter) => adapter != null);
    });
    expect(result).toBe(true);
  });
  it("requestAdapter (3)", async () => {
    const result = await client.eval(({ gpu }) => {
      return gpu.requestAdapter(undefined).then((adapter) => adapter != null);
    });
    expect(result).toBe(true);
  });
  it("requestAdapter (4)", async () => {
    const result = await client.eval(({ gpu }) => {
      return gpu.requestAdapter({}).then((adapter) => adapter != null);
    });
    expect(result).toBe(true);
  });
  it("requestAdapter (5)", async () => {
    const result = await client.eval(({ gpu }) => {
      return gpu
        .requestAdapter({ powerPreference: "low-power" })
        .then((adapter) => adapter != null);
    });
    expect(result).toBe(true);
  });
  it("isFallback", async () => {
    const result = await client.eval(({ adapter }) => {
      return adapter.isFallbackAdapter;
    });
    expect(result).toBe(false);
  });
  it("features", async () => {
    const result = await client.eval(({ adapter }) => {
      return Array.from(adapter.features);
    });
    expect(result.includes("depth-clip-control")).toBe(true);
    expect(result.includes("rg11b10ufloat-renderable")).toBe(true);
    expect(result.includes("shader-f16")).toBe(true);
    expect(result.includes("texture-compression-etc2")).toBe(true);
  });
  // it("limits", async () => {
  //   const result = await client.eval(({ adapter }) => {
  //     return adapter.limits;
  //   });
  //   expect(result).toEqual({});
  // });
  // it("requestAdapter (6)", async () => {
  //   const result = await client.eval(({ gpu }) => {
  //     return gpu.requestAdapter().then((adapter) => adapter?.limits);
  //   });
  //   expect(result).toBe(true);
  // });
  it("getPreferredCanvasFormat", async () => {
    const result = await client.eval(({ gpu }) => {
      return gpu.getPreferredCanvasFormat();
    });
    expect(result).toBe(client.OS === "android" ? "rgba8unorm" : "bgra8unorm");
  });
  it("wgslLanguageFeatures", async () => {
    const result = await client.eval(({ gpu }) => {
      return Array.from(gpu.wgslLanguageFeatures);
    });
    expect(result.includes("readonly_and_readwrite_storage_textures")).toBe(
      true,
    );
    expect(result.includes("packed_4x8_integer_dot_product")).toBe(true);
    expect(result.includes("unrestricted_pointer_parameters")).toBe(true);
    expect(result.includes("pointer_composite_access")).toBe(true);
  });
});
