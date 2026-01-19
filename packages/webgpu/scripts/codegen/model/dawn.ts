import _ from "lodash";

// eslint-disable-next-line @typescript-eslint/ban-ts-comment
// @ts-expect-error
import dawn from "../../../libs/dawn.json";

export const mapKeys = <T extends object>(obj: T) =>
  Object.keys(obj) as (keyof T)[];

export const hasPropery = <O, T extends string>(
  object: unknown,
  property: T,
): object is O & Record<T, unknown> => {
  return typeof object === "object" && object !== null && property in object;
};

interface Method {
  returnType: string;
  args: { name: string; type: string }[];
  deps: string[];
}

const setBindGroup: Method = {
  deps: ["variant", "memory", "GPUBindGroup", "vector", "optional"],
  returnType: "void",
  args: [
    { name: "index", type: "uint32_t" },
    {
      name: "bindGroup",
      type: "std::variant<std::nullptr_t, std::shared_ptr<GPUBindGroup>>",
    },
    {
      name: "dynamicOffsets",
      type: "std::optional<std::vector<uint32_t>>",
    },
  ],
};

export const resolved: Record<
  string,
  {
    extraDeps?: string[];
    extra?: string;
    ctor?: string;
    methods?: Record<string, Method>;
  }
> = {
  GPU: {
    ctor: `explicit GPU(jsi::Runtime &runtime);`,
  },
  // GPUDevice is skipped from codegen - maintained manually
  GPUBuffer: {
    methods: {
      mapAsync: {
        returnType: "async::AsyncTaskHandle",
        args: [
          { name: "modeIn", type: "uint64_t" },
          { name: "offset", type: "std::optional<uint64_t>" },
          { name: "size", type: "std::optional<uint64_t>" },
        ],
        deps: [],
      },
      getMappedRange: {
        returnType: "std::shared_ptr<ArrayBuffer>",
        args: [
          { name: "offset", type: "std::optional<size_t>" },
          { name: "size", type: "std::optional<size_t>" },
        ],
        deps: ["memory", "optional", "ArrayBuffer"],
      },
    },
    extra: `size_t getMemoryPressure() override { return static_cast<size_t>(getSize()); }

  struct Mapping {
    uint64_t start;
    uint64_t end;
    inline bool Intersects(uint64_t s, uint64_t e) const { return s < end && e > start; }
    std::shared_ptr<ArrayBuffer> buffer;
  };
  std::vector<Mapping> mappings;`,
    extraDeps: ["vector"],
  },
  GPUCommandEncoder: {
    methods: {
      copyTextureToBuffer: {
        deps: [
          "memory",
          "GPUImageCopyTexture",
          "GPUImageCopyBuffer",
          "GPUExtent3D",
        ],
        returnType: "void",
        args: [
          { name: "source", type: "std::shared_ptr<GPUImageCopyTexture>" },
          { name: "destination", type: "std::shared_ptr<GPUImageCopyBuffer>" },
          { name: "copySize", type: "std::shared_ptr<GPUExtent3D>" },
        ],
      },
      copyTextureToTexture: {
        deps: [
          "memory",
          "GPUImageCopyTexture",
          "GPUImageCopyTexture",
          "GPUExtent3D",
        ],
        returnType: "void",
        args: [
          { name: "source", type: "std::shared_ptr<GPUImageCopyTexture>" },
          { name: "destination", type: "std::shared_ptr<GPUImageCopyTexture>" },
          { name: "copySize", type: "std::shared_ptr<GPUExtent3D>" },
        ],
      },
      copyBufferToTexture: {
        deps: [
          "memory",
          "GPUImageCopyBuffer",
          "GPUImageCopyBuffer",
          "GPUExtent3D",
        ],
        returnType: "void",
        args: [
          { name: "source", type: "std::shared_ptr<GPUImageCopyBuffer>" },
          { name: "destination", type: "std::shared_ptr<GPUImageCopyTexture>" },
          { name: "copySize", type: "std::shared_ptr<GPUExtent3D>" },
        ],
      },
    },
  },
  GPUQueue: {
    methods: {
      writeBuffer: {
        deps: ["GPUBuffer", "memory", "ArrayBuffer"],
        returnType: "void",
        args: [
          { name: "buffer", type: "std::shared_ptr<GPUBuffer>" },
          { name: "bufferOffset", type: "uint64_t" },
          { name: "data", type: "std::shared_ptr<ArrayBuffer> " },
          { name: "dataOffsetElements", type: "std::optional<uint64_t>" },
          { name: "sizeElements", type: "std::optional<size_t>" },
        ],
      },
      writeTexture: {
        deps: [],
        returnType: "void",
        args: [
          { name: "destination", type: "std::shared_ptr<GPUImageCopyTexture>" },
          { name: "data", type: "std::shared_ptr<ArrayBuffer>" },
          { name: "dataLayout", type: "std::shared_ptr<GPUImageDataLayout>" },
          { name: "size", type: "std::shared_ptr<GPUExtent3D>" },
        ],
      },
      copyExternalImageToTexture: {
        deps: ["GPUImageCopyExternalImage", "GPUImageCopyTextureTagged"],
        returnType: "void",
        args: [
          {
            name: "source",
            type: "std::shared_ptr<GPUImageCopyExternalImage>",
          },
          {
            name: "destination",
            type: "std::shared_ptr<GPUImageCopyTextureTagged>",
          },
          { name: "copySize", type: "std::shared_ptr<GPUExtent3D>" },
        ],
      },
    },
  },
  GPUComputePassEncoder: {
    methods: {
      setBindGroup,
    },
  },
  GPURenderPassEncoder: {
    methods: {
      setBindGroup,
    },
  },
  GPURenderBundleEncoder: {
    methods: {
      setBindGroup,
    },
  },
  GPUComputePipeline: {
    extra: `size_t getMemoryPressure() override {
    // Compute pipelines contain compiled compute shader state and
    // driver-specific optimized code
    // Estimate: 16KB for a typical compute pipeline (single compute shader)
    return 16 * 1024;
  }

  friend class GPUDevice;`,
  },
  GPURenderPipeline: {
    extra: `size_t getMemoryPressure() override {
    // Render pipelines contain compiled shader state, vertex/fragment shaders,
    // render state, and driver-specific optimized code
    // Estimate: 24KB for a typical render pipeline with vertex + fragment shaders
    return 24 * 1024;
  }

  friend class GPUDevice;`,
  },
  GPUBindGroup: {
    extra: `size_t getMemoryPressure() override {
    // Bind groups store resource bindings and descriptor state
    // They reference buffers, textures, samplers, etc.
    // Estimate: 1KB per bind group (descriptor tables and binding state)
    return 1024;
  }`,
  },
  GPUBindGroupLayout: {
    extra: `size_t getMemoryPressure() override {
    // Bind group layouts define the structure/schema for bind groups
    // They store binding descriptors, types, and validation info
    // Estimate: 512 bytes per layout (smaller than actual bind groups)
    return 512;
  }`,
  },
  GPUQuerySet: {
    extra: `size_t getMemoryPressure() override {
    uint32_t count = getCount();
    wgpu::QueryType type = getType();

    // Estimate bytes per query based on type
    size_t bytesPerQuery = 8; // Default estimate
    switch (type) {
    case wgpu::QueryType::Occlusion:
      bytesPerQuery = 8; // 64-bit counter
      break;
    case wgpu::QueryType::Timestamp:
      bytesPerQuery = 8; // 64-bit timestamp
      break;
    default:
      bytesPerQuery = 8; // Safe default
      break;
    }

    return static_cast<size_t>(count) * bytesPerQuery;
  }`,
  },
  GPUShaderModule: {
    extra: `size_t getMemoryPressure() override {
    // Estimate memory usage for compiled shader module
    // Shaders can vary widely, but a reasonable estimate is 8-16KB for typical shaders
    // Complex shaders (with many uniforms, textures, or computations) can be much larger
    return 12 * 1024; // 12KB estimate for average shader
  }`,
  },
  GPUTexture: {
    extraDeps: ["algorithm"],
    extra: `size_t getMemoryPressure() override {
    // Calculate approximate memory usage based on texture properties
    uint32_t width = getWidth();
    uint32_t height = getHeight();
    uint32_t depthOrArrayLayers = getDepthOrArrayLayers();
    uint32_t mipLevelCount = getMipLevelCount();
    uint32_t sampleCount = getSampleCount();

    // Estimate bytes per pixel based on format
    // This is a simplified estimate - actual values depend on the specific format
    size_t bytesPerPixel = 4; // Default to RGBA8 format
    wgpu::TextureFormat format = getFormat();
    switch (format) {
    case wgpu::TextureFormat::R8Unorm:
    case wgpu::TextureFormat::R8Snorm:
    case wgpu::TextureFormat::R8Uint:
    case wgpu::TextureFormat::R8Sint:
      bytesPerPixel = 1;
      break;
    case wgpu::TextureFormat::R16Uint:
    case wgpu::TextureFormat::R16Sint:
    case wgpu::TextureFormat::R16Float:
    case wgpu::TextureFormat::RG8Unorm:
    case wgpu::TextureFormat::RG8Snorm:
    case wgpu::TextureFormat::RG8Uint:
    case wgpu::TextureFormat::RG8Sint:
      bytesPerPixel = 2;
      break;
    case wgpu::TextureFormat::RGBA8Unorm:
    case wgpu::TextureFormat::RGBA8UnormSrgb:
    case wgpu::TextureFormat::RGBA8Snorm:
    case wgpu::TextureFormat::RGBA8Uint:
    case wgpu::TextureFormat::RGBA8Sint:
    case wgpu::TextureFormat::BGRA8Unorm:
    case wgpu::TextureFormat::BGRA8UnormSrgb:
    case wgpu::TextureFormat::RGB10A2Unorm:
    case wgpu::TextureFormat::R32Float:
    case wgpu::TextureFormat::R32Uint:
    case wgpu::TextureFormat::R32Sint:
    case wgpu::TextureFormat::RG16Uint:
    case wgpu::TextureFormat::RG16Sint:
    case wgpu::TextureFormat::RG16Float:
      bytesPerPixel = 4;
      break;
    case wgpu::TextureFormat::RG32Float:
    case wgpu::TextureFormat::RG32Uint:
    case wgpu::TextureFormat::RG32Sint:
    case wgpu::TextureFormat::RGBA16Uint:
    case wgpu::TextureFormat::RGBA16Sint:
    case wgpu::TextureFormat::RGBA16Float:
      bytesPerPixel = 8;
      break;
    case wgpu::TextureFormat::RGBA32Float:
    case wgpu::TextureFormat::RGBA32Uint:
    case wgpu::TextureFormat::RGBA32Sint:
      bytesPerPixel = 16;
      break;
    default:
      bytesPerPixel = 4; // Safe default
      break;
    }

    // Calculate total memory for all mip levels
    size_t totalMemory = 0;
    for (uint32_t mip = 0; mip < mipLevelCount; ++mip) {
      uint32_t mipWidth = std::max(1u, width >> mip);
      uint32_t mipHeight = std::max(1u, height >> mip);
      totalMemory += static_cast<size_t>(mipWidth) * mipHeight *
                     depthOrArrayLayers * bytesPerPixel * sampleCount;
    }

    return totalMemory;
  }`,
  },
};

const toNativeName = (name: string) => {
  return _.upperFirst(_.camelCase(name));
};

export const resolveNative = (
  className: string,
  methodName: string,
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
): any => {
  const key = Object.keys(dawn).find(
    (k) => toNativeName(k) === className.substring(3),
  );
  const object = dawn[key as keyof typeof dawn];
  if (!object) {
    return null;
  }
  if (!hasPropery(object, "methods")) {
    return null;
  }
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  return (object.methods as any).find((m: any) => {
    return _.camelCase(m.name.toLowerCase()) === methodName;
  });
};

export const resolveExtra = (className: string) => {
  return resolved[className]?.extra ?? "";
};

export const resolveExtraDeps = (className: string) => {
  return resolved[className]?.extraDeps ?? [];
};

export const resolveCtor = (className: string): string | undefined => {
  return resolved[className]?.ctor;
};

export const resolveMethod = (className: string, methodName: string) => {
  const obj = resolved[className];
  if (obj && obj.methods) {
    return obj.methods[methodName];
  }
  return;
};
