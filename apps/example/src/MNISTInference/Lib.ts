import type { TgpuBuffer, StorageFlag, TgpuRoot } from "typegpu";
import tgpu, { d, std } from "typegpu";

export const SIZE = 28;

// Definitions for the network

type FloatsStorageBuffer = TgpuBuffer<d.WgslArray<d.F32>> & StorageFlag;

interface LayerData {
  shape: readonly [number] | readonly [number, number];
  buffer: FloatsStorageBuffer;
}

interface Layer {
  weights: FloatsStorageBuffer;
  biases: FloatsStorageBuffer;
  state: FloatsStorageBuffer;
}

export interface Network {
  layers: Layer[];
  input: FloatsStorageBuffer;
  output: FloatsStorageBuffer;

  inference(data: number[]): Promise<number[]>;
}

export const centerData = (data: Uint8Array) => {
  "worklet";
  const mass = data.reduce((acc, value) => acc + value, 0);
  const x = data.reduce((acc, value, i) => acc + value * (i % SIZE), 0) / mass;
  const y =
    data.reduce((acc, value, i) => acc + value * Math.floor(i / SIZE), 0) /
    mass;

  const offsetX = Math.round(SIZE / 2 - x);
  const offsetY = Math.round(SIZE / 2 - y);

  const newData = Array.from({ length: SIZE * SIZE }, () => 0);
  for (let i = 0; i < SIZE; i++) {
    for (let j = 0; j < SIZE; j++) {
      const index = i * SIZE + j;
      const newIndex = (i + offsetY) * SIZE + j + offsetX;
      if (newIndex >= 0 && newIndex < SIZE * SIZE) {
        newData[newIndex] = data[index];
      }
    }
  }

  return newData;
};

export const createDemo = async (root: TgpuRoot) => {
  const ReadonlyFloats = {
    storage: d.arrayOf(d.f32),
    access: "readonly",
  } as const;
  const MutableFloats = {
    storage: d.arrayOf(d.f32),
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

  function relu(x: number): number {
    "use gpu";
    return std.max(0, x);
  }

  const pipeline = root.createGuardedComputePipeline((i: number) => {
    "use gpu";
    const inputSize = ioLayout.$.input.length;
    const weightsOffset = i * inputSize;
    let sum = d.f32(0);

    for (let j = 0; j < inputSize; j++) {
      sum +=
        ioLayout.$.input[j] * weightsBiasesLayout.$.weights[weightsOffset + j];
    }

    sum += weightsBiasesLayout.$.biases[i];
    ioLayout.$.output[i] = relu(sum);
  });

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
          .createBuffer(d.arrayOf(d.f32, biases.shape[0]))
          .$usage("storage"),
      };
    });

    const input = root
      .createBuffer(d.arrayOf(d.f32, layers[0][0].shape[0]))
      .$usage("storage");
    const output = buffers[buffers.length - 1].state;

    const ioBindGroups = buffers.map((_, i) =>
      root.createBindGroup(ioLayout, {
        input: i === 0 ? input : buffers[i - 1].state,
        output: buffers[i].state,
      }),
    );

    const weightsBindGroups = buffers.map((layer) =>
      root.createBindGroup(weightsBiasesLayout, {
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
      for (let i = 0; i < buffers.length; i++) {
        pipeline
          .with(ioBindGroups[i])
          .with(weightsBindGroups[i])
          .dispatchThreads(buffers[i].biases.dataType.elementCount);
      }

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
      .createBuffer(d.arrayOf(d.f32, data.length), data)
      .$usage("storage");

    return {
      shape,
      buffer,
    };
  }

  function downloadLayers(): Promise<[LayerData, LayerData][]> {
    const downloadLayer = async (fileName: string): Promise<LayerData> => {
      const buffer = await fetch(
        `https://docs.swmansion.com/TypeGPU/assets/mnist-weights/${fileName}`,
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

  return { network };
};
