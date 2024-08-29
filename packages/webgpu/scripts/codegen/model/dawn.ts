import _ from "lodash";

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
    ctor: `GPU()
      : HybridObject("GPU")  {
          wgpu::InstanceDescriptor instanceDesc;
          instanceDesc.features.timedWaitAnyEnable = true;
          instanceDesc.features.timedWaitAnyMaxCount = 64;
          _instance = wgpu::CreateInstance(&instanceDesc);
          auto instance = &_instance;
          _async = std::make_shared<AsyncRunner>(instance);
      }`,
  },
  GPUDevice: {
    ctor: `  explicit GPUDevice(wgpu::Device instance, std::shared_ptr<AsyncRunner> async,
                     std::string label)
      : HybridObject("GPUDevice"), _instance(instance), _async(async),
        _label(label) {
          m_lostPromise = std::make_shared<std::promise<std::shared_ptr<GPUDeviceLostInfo>>>();
        }`,
    extra:
      "std::shared_ptr<std::promise<std::shared_ptr<GPUDeviceLostInfo>>>  m_lostPromise;",
  },
  GPUBuffer: {
    methods: {
      mapAsync: {
        returnType: "std::future<void>",
        args: [
          { name: "modeIn", type: "uint64_t" },
          { name: "offset", type: "std::optional<uint64_t>" },
          { name: "size", type: "std::optional<uint64_t>" },
        ],
        deps: ["future"],
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
    extra: `struct Mapping {
      uint64_t start;
      uint64_t end;
      inline bool Intersects(uint64_t s, uint64_t e) const { return s < end && e > start; }
      std::shared_ptr<ArrayBuffer> buffer;
  };
  std::vector<Mapping> mappings;
  `,
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
    extra: "friend class GPUDevice;",
  },
  GPURenderPipeline: {
    extra: "friend class GPUDevice;",
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
  return object.methods.find((m) => {
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
