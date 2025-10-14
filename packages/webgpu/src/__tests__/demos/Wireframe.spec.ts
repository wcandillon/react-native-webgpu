/* eslint-disable @typescript-eslint/ban-ts-comment */
import { modelData as model } from "../components/Wireframe/models";
import {
  solidColorLitWGSL as solidColorLit,
  wireframeWGSL as wireframe,
} from "../components/Wireframe/Shaders";
import { client } from "../setup";

type Model = {
  vertexBuffer: GPUBuffer;
  indexBuffer: GPUBuffer;
  indexFormat: GPUIndexFormat;
  vertexCount: number;
};

type ModelData = Record<
  string,
  { vertices: Float32Array; indices: Uint32Array }
>;

type SerializedModelData = Record<
  string,
  { vertices: number[]; indices: number[] }
>;

const serializeModelData = (m: ModelData): SerializedModelData => {
  const serialized: SerializedModelData = {};
  for (const [key, value] of Object.entries(m)) {
    serialized[key] = {
      vertices: Array.from(value.vertices),
      indices: Array.from(value.indices),
    };
  }
  return serialized;
};

describe("Wireframe", () => {
  it("draw scene", async () => {
    const result = await client.eval(
      ({ device, gpu, serializedData, solidColorLitWGSL, wireframeWGSL }) => {
        const parseModelData = (m: SerializedModelData): ModelData => {
          const parsed: ModelData = {};
          for (const entry of Object.entries(m)) {
            parsed[entry[0]] = {
              vertices: new Float32Array(entry[1].vertices),
              indices: new Uint32Array(entry[1].indices),
            };
          }
          return parsed;
        };

        const modelData = parseModelData(serializedData);
        const settings = {
          barycentricCoordinatesBased: false,
          thickness: 2,
          alphaThreshold: 0.5,
          animate: true,
          lines: true,
          depthBias: 1,
          depthBiasSlopeScale: 0.5,
          models: true,
        };
        function createBufferWithData(
          // eslint-disable-next-line @typescript-eslint/no-shadow
          device: GPUDevice,
          data: Float32Array<ArrayBufferLike> | Uint32Array<ArrayBufferLike>,
          usage: GPUBufferUsageFlags,
        ) {
          const buffer = device.createBuffer({
            size: data.byteLength,
            usage,
          });
          device.queue.writeBuffer(buffer, 0, data.buffer);
          return buffer;
        }

        function createVertexAndIndexBuffer(
          // eslint-disable-next-line @typescript-eslint/no-shadow
          device: GPUDevice,
          {
            vertices,
            indices,
          }: { vertices: Float32Array; indices: Uint32Array },
        ): Model {
          const vertexBuffer = createBufferWithData(
            device,
            vertices,
            GPUBufferUsage.VERTEX |
              GPUBufferUsage.STORAGE |
              GPUBufferUsage.COPY_DST,
          );
          const indexBuffer = createBufferWithData(
            device,
            indices,
            GPUBufferUsage.INDEX |
              GPUBufferUsage.STORAGE |
              GPUBufferUsage.COPY_DST,
          );
          return {
            vertexBuffer,
            indexBuffer,
            indexFormat: "uint32",
            vertexCount: indices.length,
          };
        }

        const presentationFormat = gpu.getPreferredCanvasFormat();
        const depthFormat = "depth24plus";
        // @ts-ignore
        const models = Object.values(modelData).map((data) =>
          createVertexAndIndexBuffer(device, data),
        );
        const litModule = device.createShaderModule({
          code: solidColorLitWGSL,
        });
        // @ts-ignore
        const wireframeModule = device.createShaderModule({
          code: wireframeWGSL,
        });
        const litBindGroupLayout = device.createBindGroupLayout({
          label: "lit bind group layout",
          entries: [
            {
              binding: 0,
              visibility: GPUShaderStage.VERTEX | GPUShaderStage.FRAGMENT,
              buffer: {},
            },
          ],
        });
        const litPipeline = device.createRenderPipeline({
          label: "lit pipeline",
          layout: device.createPipelineLayout({
            bindGroupLayouts: [litBindGroupLayout],
          }),
          vertex: {
            module: litModule,
            buffers: [
              {
                arrayStride: 6 * 4, // position, normal
                attributes: [
                  {
                    // position
                    shaderLocation: 0,
                    offset: 0,
                    format: "float32x3",
                  },
                  {
                    // normal
                    shaderLocation: 1,
                    offset: 3 * 4,
                    format: "float32x3",
                  },
                ],
              },
            ],
          },
          fragment: {
            module: litModule,
            targets: [{ format: presentationFormat }],
          },
          primitive: {
            cullMode: "back",
          },
          depthStencil: {
            depthWriteEnabled: true,
            depthCompare: "less",
            // Applying a depth bias can prevent aliasing from z-fighting with the
            // wireframe lines. The depth bias has to be applied to the lit meshes
            // rather that the wireframe because depthBias isn't considered when
            // drawing line or point primitives.
            depthBias: settings.depthBias,
            depthBiasSlopeScale: settings.depthBiasSlopeScale,
            format: depthFormat,
          },
        });
        return litPipeline.label;
      },
      {
        serializedData: serializeModelData(model),
        solidColorLitWGSL: solidColorLit,
        wireframeWGSL: wireframe,
      },
    );
    expect(result).toBe("lit pipeline");
    // const image = encodeImage(result);
    // checkImage(image, "snapshots/wireframe.png");
  });
});
