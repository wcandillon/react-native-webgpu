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

export const resolved: Record<
  string,
  { methods: NativeMethod[]; properties: NativeProperty[]; extra?: string }
> = {
  GPU: {
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
    extra: "friend class GPU;",
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
      std::shared_ptr<MutableJSIBuffer> buffer;
  };
  std::vector<Mapping> mappings;`,
  },
};

export const dawn = dawnJSON;
