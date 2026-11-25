import { client } from "./setup";

// Workgroup size stays well below CI SwiftShader's 256 limit (see GPU.spec.ts).
const heavyComputeWGSL = `
  @group(0) @binding(0) var<storage, read_write> out: array<u32>;

  @compute @workgroup_size(64)
  fn main(@builtin(global_invocation_id) id: vec3<u32>) {
    var acc: u32 = id.x + 1u;
    for (var i: u32 = 0u; i < 1024u; i = i + 1u) {
      acc = (acc * 1664525u + 1013904223u) ^ (acc >> 16u);
    }
    out[id.x & 63u] = acc | 1u;
  }
`;

// Regression tests for #637: mapAsync on an unrelated buffer must not block
// behind an older onSubmittedWorkDone() future on the native waiter thread.
describe("mapAsync wait order", () => {
  it("resolves mapAsync while an earlier onSubmittedWorkDone is still pending (#637)", async () => {
    const result = await client.eval(
      ({ device, shaderCode }) => {
        const storageBuffer = device.createBuffer({
          size: 256,
          usage: GPUBufferUsage.STORAGE,
        });
        const pipeline = device.createComputePipeline({
          layout: "auto",
          compute: {
            module: device.createShaderModule({ code: shaderCode }),
            entryPoint: "main",
          },
        });
        const bindGroup = device.createBindGroup({
          layout: pipeline.getBindGroupLayout(0),
          entries: [{ binding: 0, resource: { buffer: storageBuffer } }],
        });
        const maxWorkgroups = Math.min(
          device.limits.maxComputeWorkgroupsPerDimension,
          65535,
        );
        const encodeHeavy = (workgroups: number, repeats: number) => {
          const encoder = device.createCommandEncoder();
          const pass = encoder.beginComputePass();
          pass.setPipeline(pipeline);
          pass.setBindGroup(0, bindGroup);
          for (let i = 0; i < repeats; i++) {
            pass.dispatchWorkgroups(workgroups);
          }
          pass.end();
          return encoder.finish();
        };

        const calibrate = (
          workgroups: number,
          repeats: number,
          step: number,
        ): Promise<{ workgroups: number; repeats: number }> => {
          const start = Date.now();
          device.queue.submit([encodeHeavy(workgroups, repeats)]);
          return device.queue.onSubmittedWorkDone().then(() => {
            if (Date.now() - start >= 50 || step >= 20) {
              return { workgroups, repeats };
            }
            if (workgroups < maxWorkgroups) {
              return calibrate(
                Math.min(workgroups * 4, maxWorkgroups),
                repeats,
                step + 1,
              );
            }
            if (repeats < 64) {
              return calibrate(workgroups, Math.min(repeats * 2, 64), step + 1);
            }
            return { workgroups, repeats };
          });
        };

        const srcBuffer = device.createBuffer({
          size: 16,
          usage: GPUBufferUsage.COPY_SRC,
          mappedAtCreation: true,
        });
        new Uint32Array(srcBuffer.getMappedRange()).set([1, 2, 3, 4]);
        srcBuffer.unmap();

        const readbackBuffer = device.createBuffer({
          size: 16,
          usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
        });

        return calibrate(16, 1, 0).then(({ workgroups, repeats }) => {
          // Seed readbackBuffer in its own tiny prior submit so mapAsync waits
          // on an earlier serial than the heavy compute submit below.
          const copyEncoder = device.createCommandEncoder();
          copyEncoder.copyBufferToBuffer(srcBuffer, 0, readbackBuffer, 0, 16);
          device.queue.submit([copyEncoder.finish()]);

          // Heavy unrelated submit that does not touch readbackBuffer.
          device.queue.submit([encodeHeavy(workgroups, repeats)]);

          let workDoneSettled = false;
          const workDone = device.queue.onSubmittedWorkDone().then(() => {
            workDoneSettled = true;
          });

          return readbackBuffer.mapAsync(GPUMapMode.READ).then(() => {
            const settledBeforeMap = workDoneSettled;
            const { mapState } = readbackBuffer;
            const values = Array.from(
              new Uint32Array(readbackBuffer.getMappedRange()),
            );
            readbackBuffer.unmap();
            return workDone.then(() => ({
              settledBeforeMap,
              mapState,
              values,
              workDoneSettledAfter: workDoneSettled,
            }));
          });
        });
      },
      { shaderCode: heavyComputeWGSL },
    );

    expect(result.settledBeforeMap).toBe(false);
    expect(result.mapState).toBe("mapped");
    expect(result.values).toEqual([1, 2, 3, 4]);
    expect(result.workDoneSettledAfter).toBe(true);
  });

  it("resolves an idle mapAsync issued before onSubmittedWorkDone", async () => {
    const result = await client.eval(
      ({ device, shaderCode }) => {
        const storageBuffer = device.createBuffer({
          size: 256,
          usage: GPUBufferUsage.STORAGE,
        });
        const pipeline = device.createComputePipeline({
          layout: "auto",
          compute: {
            module: device.createShaderModule({ code: shaderCode }),
            entryPoint: "main",
          },
        });
        const bindGroup = device.createBindGroup({
          layout: pipeline.getBindGroupLayout(0),
          entries: [{ binding: 0, resource: { buffer: storageBuffer } }],
        });
        const maxWorkgroups = Math.min(
          device.limits.maxComputeWorkgroupsPerDimension,
          65535,
        );
        const encodeHeavy = (workgroups: number, repeats: number) => {
          const encoder = device.createCommandEncoder();
          const pass = encoder.beginComputePass();
          pass.setPipeline(pipeline);
          pass.setBindGroup(0, bindGroup);
          for (let i = 0; i < repeats; i++) {
            pass.dispatchWorkgroups(workgroups);
          }
          pass.end();
          return encoder.finish();
        };

        const calibrate = (
          workgroups: number,
          repeats: number,
          step: number,
        ): Promise<{ workgroups: number; repeats: number }> => {
          const start = Date.now();
          device.queue.submit([encodeHeavy(workgroups, repeats)]);
          return device.queue.onSubmittedWorkDone().then(() => {
            if (Date.now() - start >= 50 || step >= 20) {
              return { workgroups, repeats };
            }
            if (workgroups < maxWorkgroups) {
              return calibrate(
                Math.min(workgroups * 4, maxWorkgroups),
                repeats,
                step + 1,
              );
            }
            if (repeats < 64) {
              return calibrate(workgroups, Math.min(repeats * 2, 64), step + 1);
            }
            return { workgroups, repeats };
          });
        };

        const idleBuffer = device.createBuffer({
          size: 16,
          usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
        });

        return calibrate(16, 1, 0).then(({ workgroups, repeats }) => {
          const mapPromise = idleBuffer.mapAsync(GPUMapMode.READ);
          device.queue.submit([encodeHeavy(workgroups, repeats)]);

          let workDoneSettled = false;
          const workDone = device.queue.onSubmittedWorkDone().then(() => {
            workDoneSettled = true;
          });

          return mapPromise.then(() => {
            const settledBeforeMap = workDoneSettled;
            const { mapState } = idleBuffer;
            idleBuffer.unmap();
            return workDone.then(() => ({
              settledBeforeMap,
              mapState,
              workDoneSettledAfter: workDoneSettled,
            }));
          });
        });
      },
      { shaderCode: heavyComputeWGSL },
    );

    expect(result.settledBeforeMap).toBe(false);
    expect(result.mapState).toBe("mapped");
    expect(result.workDoneSettledAfter).toBe(true);
  });

  it("resolves multiple mapAsync calls queued behind one pending onSubmittedWorkDone", async () => {
    const result = await client.eval(
      ({ device, shaderCode }) => {
        const storageBuffer = device.createBuffer({
          size: 256,
          usage: GPUBufferUsage.STORAGE,
        });
        const pipeline = device.createComputePipeline({
          layout: "auto",
          compute: {
            module: device.createShaderModule({ code: shaderCode }),
            entryPoint: "main",
          },
        });
        const bindGroup = device.createBindGroup({
          layout: pipeline.getBindGroupLayout(0),
          entries: [{ binding: 0, resource: { buffer: storageBuffer } }],
        });
        const maxWorkgroups = Math.min(
          device.limits.maxComputeWorkgroupsPerDimension,
          65535,
        );
        const encodeHeavy = (workgroups: number, repeats: number) => {
          const encoder = device.createCommandEncoder();
          const pass = encoder.beginComputePass();
          pass.setPipeline(pipeline);
          pass.setBindGroup(0, bindGroup);
          for (let i = 0; i < repeats; i++) {
            pass.dispatchWorkgroups(workgroups);
          }
          pass.end();
          return encoder.finish();
        };

        const calibrate = (
          workgroups: number,
          repeats: number,
          step: number,
        ): Promise<{ workgroups: number; repeats: number }> => {
          const start = Date.now();
          device.queue.submit([encodeHeavy(workgroups, repeats)]);
          return device.queue.onSubmittedWorkDone().then(() => {
            if (Date.now() - start >= 50 || step >= 20) {
              return { workgroups, repeats };
            }
            if (workgroups < maxWorkgroups) {
              return calibrate(
                Math.min(workgroups * 4, maxWorkgroups),
                repeats,
                step + 1,
              );
            }
            if (repeats < 64) {
              return calibrate(workgroups, Math.min(repeats * 2, 64), step + 1);
            }
            return { workgroups, repeats };
          });
        };

        const srcBuffer = device.createBuffer({
          size: 16,
          usage: GPUBufferUsage.COPY_SRC,
          mappedAtCreation: true,
        });
        new Uint32Array(srcBuffer.getMappedRange()).set([10, 20, 30, 40]);
        srcBuffer.unmap();

        const buffers = [0, 1, 2, 3].map(() =>
          device.createBuffer({
            size: 16,
            usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
          }),
        );

        return calibrate(16, 1, 0).then(({ workgroups, repeats }) => {
          const copyEncoder = device.createCommandEncoder();
          buffers.forEach((buf) => {
            copyEncoder.copyBufferToBuffer(srcBuffer, 0, buf, 0, 16);
          });
          device.queue.submit([copyEncoder.finish()]);

          device.queue.submit([encodeHeavy(workgroups, repeats)]);

          let workDoneSettled = false;
          const workDone = device.queue.onSubmittedWorkDone().then(() => {
            workDoneSettled = true;
          });

          const mapChecks = buffers.map((buf) =>
            buf.mapAsync(GPUMapMode.READ).then(() => {
              const settledBeforeMap = workDoneSettled;
              const { mapState } = buf;
              buf.unmap();
              return { settledBeforeMap, mapState };
            }),
          );

          return Promise.all(mapChecks).then((checks) =>
            workDone.then(() => ({
              checks,
              workDoneSettledAfter: workDoneSettled,
            })),
          );
        });
      },
      { shaderCode: heavyComputeWGSL },
    );

    expect(result.checks).toEqual([
      { settledBeforeMap: false, mapState: "mapped" },
      { settledBeforeMap: false, mapState: "mapped" },
      { settledBeforeMap: false, mapState: "mapped" },
      { settledBeforeMap: false, mapState: "mapped" },
    ]);
    expect(result.workDoneSettledAfter).toBe(true);
  });

  it("still waits for the pending submit when mapping a buffer written by that submit", async () => {
    const result = await client.eval(
      ({ device, shaderCode }) => {
        const storageBuffer = device.createBuffer({
          size: 256,
          usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC,
        });
        const dependentBuffer = device.createBuffer({
          size: 256,
          usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
        });
        const pipeline = device.createComputePipeline({
          layout: "auto",
          compute: {
            module: device.createShaderModule({ code: shaderCode }),
            entryPoint: "main",
          },
        });
        const bindGroup = device.createBindGroup({
          layout: pipeline.getBindGroupLayout(0),
          entries: [{ binding: 0, resource: { buffer: storageBuffer } }],
        });

        const encoder = device.createCommandEncoder();
        const pass = encoder.beginComputePass();
        pass.setPipeline(pipeline);
        pass.setBindGroup(0, bindGroup);
        pass.dispatchWorkgroups(16);
        pass.end();
        encoder.copyBufferToBuffer(storageBuffer, 0, dependentBuffer, 0, 256);
        device.queue.submit([encoder.finish()]);

        const workDone = device.queue.onSubmittedWorkDone();
        return dependentBuffer.mapAsync(GPUMapMode.READ).then(() => {
          const { mapState } = dependentBuffer;
          const values = Array.from(
            new Uint32Array(dependentBuffer.getMappedRange()),
          );
          dependentBuffer.unmap();
          return workDone.then(() => ({
            mapState,
            allNonZero: values.every((v) => v !== 0),
          }));
        });
      },
      { shaderCode: heavyComputeWGSL },
    );

    expect(result.mapState).toBe("mapped");
    expect(result.allNonZero).toBe(true);
  });
});
