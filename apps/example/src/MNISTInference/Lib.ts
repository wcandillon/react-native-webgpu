import tgpu, { type TgpuBuffer, type Storage } from "typegpu";
import { type F32, type TgpuArray, arrayOf, f32 } from "typegpu/data";

export const SIZE = 28;
export const createDemo = async (device: GPUDevice) => {
  const root = tgpu.initFromDevice({ device });
  const canvasData = new Array<number>(SIZE ** 2).fill(0);

  // Shader code

  const layerShader = /* wgsl */ `
  @binding(0) @group(0) var<storage, read> input: array<f32>;
  @binding(1) @group(0) var<storage, read_write> output: array<f32>;

  @binding(0) @group(1) var<storage, read> weights: array<f32>;
  @binding(1) @group(1) var<storage, read> biases: array<f32>;

  fn relu(x: f32) -> f32 {
    return max(0.0, x);
  }

  @compute @workgroup_size(1)
  fn main(@builtin(global_invocation_id) gid: vec3u) {
    let inputSize = arrayLength( &input );

    let i = gid.x;

    let weightsOffset = i * inputSize;
    var sum = 0.0;

    for (var j = 0u; j < inputSize; j = j + 1) {
      sum = sum + input[j] * weights[weightsOffset + j];
    }

    sum = sum + biases[i];
    output[i] = relu(sum);
  }
`;

  const ReadonlyFloats = {
    storage: (n: number) => arrayOf(f32, n),
    access: "readonly",
  } as const;

  const MutableFloats = {
    storage: (n: number) => arrayOf(f32, n),
    access: "mutable",
  } as const;

  const ioLayout = tgpu.bindGroupLayout({
    input: ReadonlyFloats,
    output: MutableFloats,
  });

  const weightsBiasesLayout = tgpu.bindGroupLayout({
    weights: ReadonlyFloats,
    biases: ReadonlyFloats,
  });

  const pipeline = device.createComputePipeline({
    layout: device.createPipelineLayout({
      bindGroupLayouts: [
        root.unwrap(ioLayout),
        root.unwrap(weightsBiasesLayout),
      ],
    }),
    compute: {
      module: device.createShaderModule({
        code: layerShader,
      }),
    },
  });

  // Definitions for the network

  interface LayerData {
    shape: readonly [number] | readonly [number, number];
    buffer: TgpuBuffer<TgpuArray<F32>> & Storage;
  }

  interface Layer {
    weights: TgpuBuffer<TgpuArray<F32>> & Storage;
    biases: TgpuBuffer<TgpuArray<F32>> & Storage;
    state: TgpuBuffer<TgpuArray<F32>> & Storage;
  }

  interface Network {
    layers: Layer[];
    input: TgpuBuffer<TgpuArray<F32>> & Storage;
    output: TgpuBuffer<TgpuArray<F32>> & Storage;

    inference(data: number[]): Promise<number[]>;
  }

  /**
   * Creates a network from a list of pairs of weights and biases
   *
   * It automates the creation of state buffers that are used to store the intermediate results of the network
   * as well as the input layer buffer
   *
   * It provides an inference function that takes an array of input data and returns an array of output data
   */
  function createNetwork(layers: [LayerData, LayerData][]): Network {
    const buffers = layers.map(([weights, biases]) => {
      if (weights.shape[1] !== biases.shape[0]) {
        throw new Error(`Shape mismatch: ${weights.shape} and ${biases.shape}`);
      }

      return {
        weights: weights.buffer,
        biases: biases.buffer,
        state: root
          .createBuffer(arrayOf(f32, biases.shape[0]))
          .$usage("storage"),
      };
    });

    const input = root
      .createBuffer(arrayOf(f32, layers[0][0].shape[0]))
      .$usage("storage");
    const output = buffers[buffers.length - 1].state;

    const ioBindGroups = buffers.map((_, i) =>
      ioLayout.populate({
        input: i === 0 ? input : buffers[i - 1].state,
        output: buffers[i].state,
      }),
    );

    const weightsBindGroups = buffers.map((layer) =>
      weightsBiasesLayout.populate({
        weights: layer.weights,
        biases: layer.biases,
      }),
    );

    async function inference(data: number[]): Promise<number[]> {
      // verify the length of the data matches the input layer
      if (data.length !== layers[0][0].shape[0]) {
        throw new Error(
          `Data length ${data.length} does not match input shape ${layers[0][0].shape[0]}`,
        );
      }
      input.write(data);

      // Run the network
      const encoder = device.createCommandEncoder();
      for (let i = 0; i < buffers.length; i++) {
        const pass = encoder.beginComputePass();
        pass.setPipeline(pipeline);
        pass.setBindGroup(0, root.unwrap(ioBindGroups[i]));
        pass.setBindGroup(1, root.unwrap(weightsBindGroups[i]));
        pass.dispatchWorkgroups(buffers[i].biases.dataType.size); //.length
        pass.end();
      }
      device.queue.submit([encoder.finish()]);
      await device.queue.onSubmittedWorkDone();

      // Read the output
      return await output.read();
    }

    return {
      layers: buffers,
      input,
      output,
      inference,
    };
  }

  const network = createNetwork(await downloadLayers());

  // #region Downloading weights & biases

  /**
   * Create a LayerData object from a layer ArrayBuffer
   *
   * The function extracts the header, shape and data from the layer
   * If there are any issues with the layer, an error is thrown
   *
   * Automatically creates appropriate buffer initialized with the data
   */
  function getLayerData(layer: ArrayBuffer): LayerData {
    const headerLen = new Uint16Array(layer.slice(8, 10));

    const header = new TextDecoder().decode(
      new Uint8Array(layer.slice(10, 10 + headerLen[0])),
    );

    // shape can be found in the header in the format: 'shape': (x, y) or 'shape': (x,) for bias
    const shapeMatch = header.match(/'shape': \((\d+), ?(\d+)?\)/);
    if (!shapeMatch) {
      throw new Error("Shape not found in header");
    }

    // To accommodate .npy weirdness - if we have a 2d shape we need to switch the order
    const X = Number.parseInt(shapeMatch[1], 10);
    const Y = Number.parseInt(shapeMatch[2], 10);
    const shape = Number.isNaN(Y) ? ([X] as const) : ([Y, X] as const);

    const data = new Float32Array(layer.slice(10 + headerLen[0]));

    // Verify the length of the data matches the shape
    if (data.length !== shape[0] * (shape[1] || 1)) {
      throw new Error(
        `Data length ${data.length} does not match shape ${shape}`,
      );
    }

    const buffer = root
      .createBuffer(arrayOf(f32, data.length), [...data])
      .$usage("storage");

    return {
      shape,
      buffer,
    };
  }

  function downloadLayers(): Promise<[LayerData, LayerData][]> {
    const downloadLayer = async (fileName: string): Promise<LayerData> => {
      const buffer = await fetch(
        `/TypeGPU/assets/mnist-weights/${fileName}`,
      ).then((res) => res.arrayBuffer());

      return getLayerData(buffer);
    };

    return Promise.all(
      [0, 1, 2, 3, 4, 5, 6, 7].map((layer) =>
        Promise.all([
          downloadLayer(`layer${layer}.weight.npy`),
          downloadLayer(`layer${layer}.bias.npy`),
        ]),
      ),
    );
  }

  // #endregion

  // #region User Interface

  // #endregion

  // #region Resource cleanup

  function onCleanup() {
    root.destroy();
  }

  // #endregion
};
