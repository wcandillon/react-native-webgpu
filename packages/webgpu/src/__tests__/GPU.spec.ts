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
  it("requestAdapter (6)", async () => {
    const result = await client.eval(({ gpu }) => {
      return gpu.requestAdapter().then((adapter) => {
        if (!adapter) {
          return null;
        }
        return {
          vendor: adapter.info.vendor,
          architecture: adapter.info.architecture,
          device: adapter.info.device,
          description: adapter.info.description,
        };
      });
    });
    expect(result).toBeDefined();
    expect(result!.vendor).toBeDefined();
    expect(result!.architecture).toBeDefined();
    expect(result!.device).toBeDefined();
    expect(result!.description).toBeDefined();
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
    expect(result.includes("texture-compression-etc2")).toBe(true);
  });
  // it("requiredLimits", async () => {
  //   const result = await client.eval(({ adapter }) => {
  //     return adapter
  //       .requestDevice({
  //         requiredLimits: {
  //           maxBufferSize: 1024 * 1024 * 4,
  //         },
  //       })
  //       .then((device) => device.limits.maxBufferSize);
  //   });
  //   expect(result).toBe(1024 * 1024 * 4);
  // });
  // TODO: re-enable
  // it("timestamp", async () => {
  //   const result = await client.eval(({ adapter }) => {
  //     return adapter.features.has("timestamp-query");
  //   });
  //   expect(result).toBe(true);
  // });
  // it("request device with timestamp queries", async () => {
  //   const result = await client.eval(({ adapter }) => {
  //     return adapter
  //       .requestDevice({
  //         requiredFeatures: ["timestamp-query"],
  //       })
  //       .then((device) => device.features.has("timestamp-query"));
  //   });
  //   expect(result).toBe(true);
  // });
  it("limits", async () => {
    const result = await client.eval(({ adapter }) => {
      const {
        maxTextureDimension1D,
        maxTextureDimension2D,
        maxTextureDimension3D,
        maxTextureArrayLayers,
        maxBindGroups,
        maxBindGroupsPlusVertexBuffers,
        maxBindingsPerBindGroup,
        maxDynamicUniformBuffersPerPipelineLayout,
        maxDynamicStorageBuffersPerPipelineLayout,
        maxSampledTexturesPerShaderStage,
        maxSamplersPerShaderStage,
        maxStorageBuffersPerShaderStage,
        maxStorageTexturesPerShaderStage,
        maxUniformBuffersPerShaderStage,
        maxUniformBufferBindingSize,
        maxStorageBufferBindingSize,
        minUniformBufferOffsetAlignment,
        minStorageBufferOffsetAlignment,
        maxVertexBuffers,
        maxBufferSize,
        maxVertexAttributes,
        maxVertexBufferArrayStride,
        maxInterStageShaderComponents,
        maxInterStageShaderVariables,
        maxColorAttachments,
        maxColorAttachmentBytesPerSample,
        maxComputeWorkgroupStorageSize,
        maxComputeInvocationsPerWorkgroup,
        maxComputeWorkgroupSizeX,
        maxComputeWorkgroupSizeY,
        maxComputeWorkgroupSizeZ,
        maxComputeWorkgroupsPerDimension,
      } = adapter.limits;
      return {
        __brand: adapter.limits.__brand,
        maxTextureDimension1D,
        maxTextureDimension2D,
        maxTextureDimension3D,
        maxTextureArrayLayers,
        maxBindGroups,
        maxBindGroupsPlusVertexBuffers,
        maxBindingsPerBindGroup,
        maxDynamicUniformBuffersPerPipelineLayout,
        maxDynamicStorageBuffersPerPipelineLayout,
        maxSampledTexturesPerShaderStage,
        maxSamplersPerShaderStage,
        maxStorageBuffersPerShaderStage,
        maxStorageTexturesPerShaderStage,
        maxUniformBuffersPerShaderStage,
        maxUniformBufferBindingSize,
        maxStorageBufferBindingSize,
        minUniformBufferOffsetAlignment,
        minStorageBufferOffsetAlignment,
        maxVertexBuffers,
        maxBufferSize,
        maxVertexAttributes,
        maxVertexBufferArrayStride,
        maxInterStageShaderComponents,
        maxInterStageShaderVariables,
        maxColorAttachments,
        maxColorAttachmentBytesPerSample,
        maxComputeWorkgroupStorageSize,
        maxComputeInvocationsPerWorkgroup,
        maxComputeWorkgroupSizeX,
        maxComputeWorkgroupSizeY,
        maxComputeWorkgroupSizeZ,
        maxComputeWorkgroupsPerDimension,
      };
    });
    if (result.__brand) {
      expect(result.__brand).toBe("GPUSupportedLimits");
    }
    expect(result.maxBindGroups).toBeGreaterThan(0);
    expect(result.maxBindGroupsPlusVertexBuffers).toBeGreaterThan(
      result.maxBindGroups,
    );
    expect(result.maxBindingsPerBindGroup).toBeGreaterThan(0);
    expect(result.maxBufferSize).toBeGreaterThan(0);
    expect(result.maxColorAttachmentBytesPerSample).toBeGreaterThan(0);
    expect(result.maxColorAttachments).toBeGreaterThan(0);
    expect(result.maxComputeInvocationsPerWorkgroup).toBeGreaterThan(0);
    expect(result.maxComputeWorkgroupSizeX).toBeGreaterThan(0);
    expect(result.maxComputeWorkgroupSizeY).toBeGreaterThan(0);
    expect(result.maxComputeWorkgroupSizeZ).toBeGreaterThan(0);
    expect(result.maxComputeWorkgroupStorageSize).toBeGreaterThan(0);
    expect(result.maxComputeWorkgroupsPerDimension).toBeGreaterThan(0);

    // For alignment values, check if they're powers of 2
    expect(Math.log2(result.minStorageBufferOffsetAlignment) % 1).toBe(0);
    expect(Math.log2(result.minUniformBufferOffsetAlignment) % 1).toBe(0);

    // For maximum dimensions, you might want to set a reasonable lower bound
    expect(result.maxTextureDimension1D).toBeGreaterThanOrEqual(2048);
    expect(result.maxTextureDimension2D).toBeGreaterThanOrEqual(2048);
    expect(result.maxTextureDimension3D).toBeGreaterThanOrEqual(256);
  });
  // it("adapter info", async () => {
  //   const result = await client.eval(({ adapter }) => {
  //     return {
  //       __brand: adapter.info.__brand,
  //       description: adapter.info.description,
  //       architecture: adapter.info.architecture,
  //       device: adapter.info.device,
  //       vendor: adapter.info.vendor,
  //     };
  //   });
  //   expect(result.description).toBeTruthy();
  //   expect(result.architecture).toBeTruthy();
  //   expect(result.device).toBeTruthy();
  //   expect(result.vendor).toBeTruthy();
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
