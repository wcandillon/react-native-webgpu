import { client } from "./setup";

describe("Adapter", () => {
  it("executes a simple  function", async () => {
    const result = await client.eval(() => 1 + 1);
    expect(result).toBe(2);
  });
  it("execute a simple async function (1)", async () => {
    const result = await client.eval(
      () => new Promise((resolve) => setTimeout(() => resolve(1 + 1), 100)),
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
  // it("requestAdapter (6)", async () => {
  //   const result = await client.eval(({ gpu }) => {
  //     return gpu.requestAdapter().then((adapter) => adapter?.limits);
  //   });
  //   expect(result).toBe(true);
  // });
});