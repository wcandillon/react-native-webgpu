/* eslint-disable prefer-destructuring */
import { client } from "./setup";

const multiplyMatrices = (m1: number[], m2: number[]) => {
  const rows1 = m1[0],
    cols1 = m1[1];
  const cols2 = m2[1];
  const result: number[] = new Array(2 + rows1 * cols2);
  result[0] = rows1;
  result[1] = cols2;

  for (let i = 0; i < rows1; i++) {
    for (let j = 0; j < cols2; j++) {
      let sum = 0;
      for (let k = 0; k < cols1; k++) {
        sum += m1[2 + i * cols1 + k] * m2[2 + k * cols2 + j];
      }
      result[2 + i * cols2 + j] = sum;
    }
  }
  return result;
};

describe("Computer Shader", () => {
  it("matrix multiplication", async () => {
    // First Matrix
    const rows = 16;
    const columns = 16;
    const m1: number[] = new Array(rows * columns + 2);
    m1[0] = rows;
    m1[1] = columns;
    for (let i = 2; i < m1.length; i++) {
      m1[i] = Math.random();
    }

    // Second Matrix
    const m2: number[] = new Array(rows * columns + 2);
    m2[0] = rows;
    m2[1] = columns;
    for (let i = 2; i < m2.length; i++) {
      m2[i] = Math.random();
    }

    const result = await client.eval(
      ({ device, firstMatrixRaw, secondMatrixRaw, rows1, columns1 }) => {
        const firstMatrix = new Float32Array(firstMatrixRaw);
        const secondMatrix = new Float32Array(secondMatrixRaw);
        const gpuBufferFirstMatrix = device.createBuffer({
          mappedAtCreation: true,
          size: firstMatrix.byteLength,
          usage: GPUBufferUsage.STORAGE,
        });
        const arrayBufferFirstMatrix = gpuBufferFirstMatrix.getMappedRange();
        new Float32Array(arrayBufferFirstMatrix).set(firstMatrix);
        gpuBufferFirstMatrix.unmap();

        const gpuBufferSecondMatrix = device.createBuffer({
          mappedAtCreation: true,
          size: secondMatrix.byteLength,
          usage: GPUBufferUsage.STORAGE,
        });
        const arrayBufferSecondMatrix = gpuBufferSecondMatrix.getMappedRange();
        new Float32Array(arrayBufferSecondMatrix).set(secondMatrix);
        gpuBufferSecondMatrix.unmap();

        // Result Matrix
        const resultMatrixBufferSize =
          Float32Array.BYTES_PER_ELEMENT *
          (2 + firstMatrix[0] * secondMatrix[1]);
        const resultMatrixBuffer = device.createBuffer({
          size: resultMatrixBufferSize,
          usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC,
        });

        // Compute shader code
        const shaderModule = device.createShaderModule({
          code: `
      struct Matrix {
        size : vec2<f32>,
        numbers: array<f32>,
      }

      @group(0) @binding(0) var<storage, read> firstMatrix : Matrix;
      @group(0) @binding(1) var<storage, read> secondMatrix : Matrix;
      @group(0) @binding(2) var<storage, read_write> resultMatrix : Matrix;

      @compute @workgroup_size(8, 8)
      fn main(@builtin(global_invocation_id) global_id : vec3<u32>) {
        // Guard against out-of-bounds work group sizes
        if (global_id.x >= u32(firstMatrix.size.x) || global_id.y >= u32(secondMatrix.size.y)) {
          return;
        }

        resultMatrix.size = vec2(firstMatrix.size.x, secondMatrix.size.y);

        let resultCell = vec2(global_id.x, global_id.y);
        var result = 0.0;
        for (var i = 0u; i < u32(firstMatrix.size.y); i = i + 1u) {
          let a = i + resultCell.x * u32(firstMatrix.size.y);
          let b = resultCell.y + i * u32(secondMatrix.size.y);
          result = result + firstMatrix.numbers[a] * secondMatrix.numbers[b];
        }

        let index = resultCell.y + resultCell.x * u32(secondMatrix.size.y);
        resultMatrix.numbers[index] = result;
      }
    `,
        });

        // Pipeline setup
        const computePipeline = device.createComputePipeline({
          layout: "auto",
          compute: {
            module: shaderModule,
            entryPoint: "main",
          },
        });

        // Bind group
        const bindGroup = device.createBindGroup({
          layout: computePipeline.getBindGroupLayout(0 /* index */),
          entries: [
            {
              binding: 0,
              resource: {
                buffer: gpuBufferFirstMatrix,
              },
            },
            {
              binding: 1,
              resource: {
                buffer: gpuBufferSecondMatrix,
              },
            },
            {
              binding: 2,
              resource: {
                buffer: resultMatrixBuffer,
              },
            },
          ],
        });

        // Commands submission
        const commandEncoder = device.createCommandEncoder();

        const passEncoder = commandEncoder.beginComputePass();
        passEncoder.setPipeline(computePipeline);
        passEncoder.setBindGroup(0, bindGroup);
        const workgroupCountX = Math.ceil(rows1 / 8);
        const workgroupCountY = Math.ceil(columns1 / 8);
        passEncoder.dispatchWorkgroups(workgroupCountX, workgroupCountY);
        passEncoder.end();

        // Get a GPU buffer for reading in an unmapped state.
        const gpuReadBuffer = device.createBuffer({
          size: resultMatrixBufferSize,
          usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
        });

        // Encode commands for copying buffer to buffer.
        commandEncoder.copyBufferToBuffer(
          resultMatrixBuffer /* source buffer */,
          0 /* source offset */,
          gpuReadBuffer /* destination buffer */,
          0 /* destination offset */,
          resultMatrixBufferSize /* size */,
        );

        // Submit GPU commands.
        const gpuCommands = commandEncoder.finish();
        device.queue.submit([gpuCommands]);

        // Read buffer.
        return gpuReadBuffer.mapAsync(GPUMapMode.READ).then(() => {
          const arrayBuffer = gpuReadBuffer.getMappedRange();
          const uint8Array = new Float32Array(arrayBuffer);
          // At this point, uint8Array contains the pixel data of the rendered image
          // You can process it further, save it, or send it somewhere else
          const r = Array.from(uint8Array);
          gpuReadBuffer.unmap();
          return r;
        });
      },
      {
        firstMatrixRaw: m1,
        secondMatrixRaw: m2,
        rows1: rows,
        columns1: columns,
      },
    );
    expect(result.length).toBe(16 * 16 + 2);
    const referenceResult = multiplyMatrices(m1, m2);
    for (let i = 0; i < result.length; i++) {
      expect(result[i]).toBeCloseTo(referenceResult[i], 5); // Using 5 decimal places for floating-point comparison
    }
  });
});
