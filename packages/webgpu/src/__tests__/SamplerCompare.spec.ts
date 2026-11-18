import { client } from "./setup";

// Regression test for #624: on the iOS Simulator, Dawn turns on
// metal_disable_sampler_compare (the Simulator only advertises GPUFamily2), so
// creating a sampler with a compare function is a validation error that also
// loses the device. Compare samplers are core WebGPU (shadow maps), so this
// must work on every backend, the Simulator included.
describe("Sampler compare", () => {
  it("samples a depth texture with a compare sampler", async () => {
    const result = await client.eval(({ gpu }) => {
      // A fresh device: on failure it is lost, and the shared test device must
      // stay usable for the other specs.
      return gpu
        .requestAdapter()
        .then((adapter) => adapter!.requestDevice())
        .then((device) => {
          let lost: string | null = null;
          device.lost.then((info) => {
            lost = info.message;
          });

          // Two depth texels: 0.25 and 0.75. depth16unorm is a valid
          // writeTexture destination (depth32float is not).
          const depth = device.createTexture({
            size: [2, 1],
            format: "depth16unorm",
            usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST,
          });
          device.queue.writeTexture(
            { texture: depth },
            new Uint16Array([16384, 49152]),
            { bytesPerRow: 4 },
            [2, 1],
          );

          device.pushErrorScope("validation");
          const sampler = device.createSampler({ compare: "less" });
          const out = device.createBuffer({
            size: 8,
            usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC,
          });
          const readback = device.createBuffer({
            size: 8,
            usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
          });
          const pipeline = device.createComputePipeline({
            layout: "auto",
            compute: {
              module: device.createShaderModule({
                code: `
@group(0) @binding(0) var depthTex: texture_depth_2d;
@group(0) @binding(1) var cmp: sampler_comparison;
@group(0) @binding(2) var<storage, read_write> result: array<f32, 2>;

@compute @workgroup_size(1)
fn main() {
  // "less": 1.0 when the reference (0.5) is less than the stored depth.
  result[0] = textureSampleCompareLevel(depthTex, cmp, vec2f(0.25, 0.5), 0.5);
  result[1] = textureSampleCompareLevel(depthTex, cmp, vec2f(0.75, 0.5), 0.5);
}`,
              }),
              entryPoint: "main",
            },
          });
          const bindGroup = device.createBindGroup({
            layout: pipeline.getBindGroupLayout(0),
            entries: [
              { binding: 0, resource: depth.createView() },
              { binding: 1, resource: sampler },
              { binding: 2, resource: { buffer: out } },
            ],
          });
          const encoder = device.createCommandEncoder();
          const pass = encoder.beginComputePass();
          pass.setPipeline(pipeline);
          pass.setBindGroup(0, bindGroup);
          pass.dispatchWorkgroups(1);
          pass.end();
          encoder.copyBufferToBuffer(out, 0, readback, 0, 8);
          device.queue.submit([encoder.finish()]);

          return device.popErrorScope().then((error) => {
            const errorMessage = error ? error.message : null;
            return readback.mapAsync(GPUMapMode.READ).then(
              () => {
                const values = Array.from(
                  new Float32Array(readback.getMappedRange()),
                );
                readback.unmap();
                device.destroy();
                return { errorMessage, lost, values };
              },
              (e: Error) => ({
                errorMessage: errorMessage || e.message,
                lost,
                values: [] as number[],
              }),
            );
          });
        });
    });
    // On failure the device is lost first, so the lost message carries the
    // actual cause (the error scope and mapAsync only see the aftermath).
    expect(result.lost).toBeNull();
    expect(result.errorMessage).toBeNull();
    expect(result.values).toEqual([0, 1]);
  });
});
