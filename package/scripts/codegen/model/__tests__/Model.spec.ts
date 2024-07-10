import { resolveMethod } from "../model";

describe("Model", () => {
  it("Instance", () => {
    const method = resolveMethod("GPU", "requestAdapter");
    expect(method).toBeTruthy();
    //expect(method.returns).toBe("std::future<std::shared_ptr<GPUAdapter>>");
  });
  it("Returns enum type", () => {
    const method = resolveMethod("GPUSurface", "getPreferredFormat");
    expect(method.returns).toBe("wgpu::TextureFormat");
  });
});
