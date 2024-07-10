import { resolveMethod } from "../model";

describe("Model", () => {
  it("Returns enum type", () => {
    const method = resolveMethod("GPUSurface", "getPreferredFormat");
    expect(method.returns).toBe("wgpu::TextureFormat");
  });
});
