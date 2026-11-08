import { client } from "./setup";

// A null entry in a sequence of slots leaves that slot empty: the entries
// after it keep their index (vertex buffers, fragment targets, color
// attachments and a render bundle's color formats).
describe("Null slots", () => {
  it("keeps the index of a vertex buffer after a null one", async () => {
    const result = await client.eval(({ device }) => {
      const module = device.createShaderModule({
        code: `
          @vertex fn vs(@location(0) p: vec2f) -> @builtin(position) vec4f {
            return vec4f(p, 0, 1);
          }
          @fragment fn fs() -> @location(0) vec4f { return vec4f(1, 0, 0, 1); }
        `,
      });
      const target = device.createTexture({
        size: [4, 4],
        format: "rgba8unorm",
        usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
      });
      // One triangle that covers the whole target.
      const positions = new Float32Array([-1, -1, 3, -1, -1, 3]);
      const vertices = device.createBuffer({
        size: positions.byteLength,
        usage: GPUBufferUsage.VERTEX | GPUBufferUsage.COPY_DST,
      });
      device.queue.writeBuffer(vertices, 0, positions);
      const readback = device.createBuffer({
        size: 256 * 4,
        usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
      });
      device.pushErrorScope("validation");
      const pipeline = device.createRenderPipeline({
        layout: "auto",
        vertex: {
          module,
          buffers: [
            null,
            {
              arrayStride: 8,
              attributes: [
                { shaderLocation: 0, offset: 0, format: "float32x2" },
              ],
            },
          ],
        },
        fragment: { module, targets: [{ format: "rgba8unorm" }] },
      });
      const encoder = device.createCommandEncoder();
      const pass = encoder.beginRenderPass({
        colorAttachments: [
          {
            view: target.createView(),
            clearValue: [0, 0, 0, 0],
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      });
      pass.setPipeline(pipeline);
      pass.setVertexBuffer(1, vertices);
      pass.draw(3);
      pass.end();
      encoder.copyTextureToBuffer(
        { texture: target },
        { buffer: readback, bytesPerRow: 256 },
        [4, 4],
      );
      device.queue.submit([encoder.finish()]);
      return device.popErrorScope().then((error) =>
        readback.mapAsync(GPUMapMode.READ).then(() => {
          const pixel = Array.from(
            new Uint8Array(readback.getMappedRange(0, 4)),
          );
          readback.unmap();
          return { error: error ? error.message : null, pixel };
        }),
      );
    });
    expect(result.error).toBeNull();
    expect(result.pixel).toEqual([255, 0, 0, 255]);
  });

  it("keeps the index of a fragment target and a color attachment after a null one", async () => {
    const result = await client.eval(({ device }) => {
      const module = device.createShaderModule({
        code: `
          @vertex fn vs() -> @builtin(position) vec4f { return vec4f(0, 0, 0, 1); }
          @fragment fn fs() -> @location(1) vec4f { return vec4f(1, 0, 0, 1); }
        `,
      });
      const target = device.createTexture({
        size: [4, 4],
        format: "rgba8unorm",
        usage: GPUTextureUsage.RENDER_ATTACHMENT,
      });
      device.pushErrorScope("validation");
      const pipeline = device.createRenderPipeline({
        layout: "auto",
        vertex: { module },
        fragment: { module, targets: [null, { format: "rgba8unorm" }] },
      });
      const encoder = device.createCommandEncoder();
      const pass = encoder.beginRenderPass({
        colorAttachments: [
          null,
          { view: target.createView(), loadOp: "clear", storeOp: "store" },
        ],
      });
      pass.setPipeline(pipeline);
      pass.draw(1);
      pass.end();
      device.queue.submit([encoder.finish()]);
      return device
        .popErrorScope()
        .then((error) => ({ error: error ? error.message : null }));
    });
    expect(result.error).toBeNull();
  });

  it("keeps the index of a render bundle's color format after a null one", async () => {
    const result = await client.eval(({ device }) => {
      const module = device.createShaderModule({
        code: `
          @vertex fn vs() -> @builtin(position) vec4f { return vec4f(0, 0, 0, 1); }
          @fragment fn fs() -> @location(1) vec4f { return vec4f(1, 0, 0, 1); }
        `,
      });
      device.pushErrorScope("validation");
      const pipeline = device.createRenderPipeline({
        layout: "auto",
        vertex: { module },
        fragment: { module, targets: [null, { format: "rgba8unorm" }] },
      });
      const bundleEncoder = device.createRenderBundleEncoder({
        colorFormats: [null, "rgba8unorm"],
      });
      bundleEncoder.setPipeline(pipeline);
      bundleEncoder.draw(1);
      bundleEncoder.finish();
      return device
        .popErrorScope()
        .then((error) => ({ error: error ? error.message : null }));
    });
    expect(result.error).toBeNull();
  });
});
