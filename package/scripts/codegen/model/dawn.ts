import dawnJSON from "../../../libs/dawn.json";

interface NativeMethod {
  dependencies: string[];
  name: string;
  args: {
    name: string;
    type: string;
  }[];
  returns: string;
}

interface NativeProperty {
  dependencies: string[];
  name: string;
  returns: string;
}

const GPUBindingCommandsMixin = [
  {
    name: "setBindGroup",
    dependencies: ["GPUBindGroup"],
    returns: "void",
    args: [
      { name: "groupIndex", type: "uint32_t" },
      { name: "group", type: "std::shared_ptr<GPUBindGroup>" },
      {
        name: "dynamicOffsets",
        type: "std::optional<std::vector<uint32_t>>",
      },
    ],
  },
];

export const mapKeys = <T extends object>(obj: T) =>
  Object.keys(obj) as (keyof T)[];

export const hasPropery = <O, T extends string>(
  object: unknown,
  property: T,
): object is O & Record<T, unknown> => {
  return typeof object === "object" && object !== null && property in object;
};

export const resolved: Record<
  string,
  {
    methods: NativeMethod[];
    properties: NativeProperty[];
    extra?: string;
    ctor?: string;
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
    properties: [],
    methods: [
      {
        name: "getPreferredCanvasFormat",
        returns: "wgpu::TextureFormat",
        args: [],
        dependencies: [],
      },
      {
        name: "requestAdapter",
        args: [
          {
            name: "options",
            type: "std::shared_ptr<GPURequestAdapterOptions>",
          },
        ],
        returns: "std::future<std::shared_ptr<GPUAdapter>>",
        dependencies: ["GPURequestAdapterOptions", "GPUAdapter"],
      },
    ],
  },
  GPUAdapter: {
    properties: [],
    methods: [
      {
        name: "requestDevice",
        args: [
          {
            name: "options",
            type: "std::shared_ptr<GPUDeviceDescriptor>",
          },
        ],
        returns: "std::future<std::shared_ptr<GPUDevice>>",
        dependencies: ["GPUDeviceDescriptor", "GPUDevice"],
      },
    ],
  },
  GPUQueue: {
    properties: [],
    methods: [
      {
        name: "writeBuffer",
        dependencies: ["GPUBuffer"],
        returns: "void",
        args: [
          {
            name: "buffer",
            type: "std::shared_ptr<GPUBuffer>",
          },
          {
            name: "bufferOffset",
            type: "uint64_t",
          },
          {
            name: "data",
            type: "std::shared_ptr<ArrayBuffer>",
          },
          {
            name: "dataOffset",
            type: "std::optional<uint64_t>",
          },
          {
            name: "size",
            type: "std::optional<size_t>",
          },
        ],
      },
      {
        name: "submit",
        dependencies: ["GPUCommandBuffer"],
        args: [
          {
            name: "commandBuffers",
            type: "std::vector<std::shared_ptr<GPUCommandBuffer>>",
          },
        ],
        returns: "void",
      },
    ],
  },
  GPUComputePassEncoder: {
    properties: [],
    methods: [...GPUBindingCommandsMixin],
  },
  GPURenderPassEncoder: {
    properties: [],
    methods: [...GPUBindingCommandsMixin],
  },
  GPURenderBundleEncoder: {
    properties: [],
    methods: [...GPUBindingCommandsMixin],
  },
  GPUBuffer: {
    properties: [
      {
        name: "size",
        returns: "size_t",
        dependencies: [],
      },
      {
        name: "usage",
        returns: "double",
        dependencies: [],
      },
      {
        name: "mapState",
        returns: "wgpu::BufferMapState",
        dependencies: [],
      },
    ],
    methods: [
      {
        name: "mapAsync",
        returns: "std::future<void>",
        dependencies: [],
        args: [
          {
            name: "mode",
            type: "uint64_t",
          },
          {
            name: "offset",
            type: "std::optional<size_t>",
          },
          {
            name: "size",
            type: "std::optional<size_t>",
          },
        ],
      },
    ],
    extra: `struct Mapping {
      uint64_t start;
      uint64_t end;
      inline bool Intersects(uint64_t s, uint64_t e) const { return s < end && e > start; }
      std::shared_ptr<ArrayBuffer> buffer;
  };
  std::vector<Mapping> mappings;
  `,
  },
};

export const dawn = dawnJSON;
