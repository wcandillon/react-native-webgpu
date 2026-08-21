import { client } from "./setup";

describe("readSync", () => {
  it("reads back compute results synchronously, same tick", async () => {
    const result = await client.eval(({ device }) => {
      const storage = device.createBuffer({
        size: 16,
        usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC,
      });
      const staging = device.createBuffer({
        size: 16,
        usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
      });
      const module = device.createShaderModule({
        code: `
@group(0) @binding(0) var<storage, read_write> out: array<u32, 4>;
@compute @workgroup_size(1)
fn main() {
  out[0] = 1u;
  out[1] = 2u;
  out[2] = 3u;
  out[3] = 42u;
}`,
      });
      const pipeline = device.createComputePipeline({
        layout: "auto",
        compute: { module, entryPoint: "main" },
      });
      const bindGroup = device.createBindGroup({
        layout: pipeline.getBindGroupLayout(0),
        entries: [{ binding: 0, resource: { buffer: storage } }],
      });
      const encoder = device.createCommandEncoder();
      const pass = encoder.beginComputePass();
      pass.setPipeline(pipeline);
      pass.setBindGroup(0, bindGroup);
      pass.dispatchWorkgroups(1);
      pass.end();
      encoder.copyBufferToBuffer(storage, 0, staging, 0, 16);
      device.queue.submit([encoder.finish()]);
      // No await between submit and read: the readback is synchronous.
      return Array.from(new Uint32Array(staging.readSync()));
    });
    expect(result).toEqual([1, 2, 3, 42]);
  });
  it("respects offset and size", async () => {
    const result = await client.eval(({ device }) => {
      const staging = device.createBuffer({
        size: 16,
        usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
      });
      device.queue.writeBuffer(
        staging,
        0,
        new Uint32Array([10, 20, 30, 40]).buffer,
      );
      return Array.from(new Uint32Array(staging.readSync(8, 8)));
    });
    expect(result).toEqual([30, 40]);
  });
  it("is repeatable on the same buffer", async () => {
    const result = await client.eval(({ device }) => {
      const staging = device.createBuffer({
        size: 4,
        usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
      });
      const reads: number[] = [];
      for (let i = 0; i < 3; i++) {
        device.queue.writeBuffer(staging, 0, new Uint32Array([i]).buffer);
        reads.push(new Uint32Array(staging.readSync())[0]!);
      }
      return reads;
    });
    expect(result).toEqual([0, 1, 2]);
  });
  it("rejects reads above the size cap", async () => {
    const result = await client.eval(({ device }) => {
      const staging = device.createBuffer({
        size: 2 * 1024 * 1024,
        usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
      });
      try {
        staging.readSync();
        return "no error";
      } catch (e) {
        return e instanceof Error && e.message.includes("limited to 1 MiB")
          ? "capped"
          : "wrong error";
      }
    });
    expect(result).toBe("capped");
  });
  it("accepts a configurable timeout", async () => {
    const result = await client.eval(({ device }) => {
      const staging = device.createBuffer({
        size: 4,
        usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
      });
      device.queue.writeBuffer(staging, 0, new Uint32Array([42]).buffer);
      return new Uint32Array(staging.readSync(undefined, undefined, 5_000))[0];
    });
    expect(result).toBe(42);
  });
  it("rejects an invalid timeout", async () => {
    const result = await client.eval(({ device }) => {
      const staging = device.createBuffer({
        size: 4,
        usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
      });
      try {
        staging.readSync(undefined, undefined, -1);
        return "no error";
      } catch (e) {
        return e instanceof Error && e.message.includes("timeoutMs")
          ? "invalid timeout"
          : "wrong error";
      }
    });
    expect(result).toBe("invalid timeout");
  });
});
