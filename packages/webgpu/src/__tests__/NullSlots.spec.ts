import { client } from "./setup";

// A null or undefined entry in a sequence of slots leaves that slot empty: the
// entries after it keep their index (vertex buffers, fragment targets, color
// attachments, a render bundle's color formats and bind group layouts).
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

  it("treats undefined and holes like null", async () => {
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
      const attachment = {
        view: target.createView(),
        loadOp: "clear" as const,
        storeOp: "store" as const,
      };
      device.pushErrorScope("validation");
      const pipeline = device.createRenderPipeline({
        layout: "auto",
        vertex: { module, buffers: [undefined] },
        fragment: { module, targets: [undefined, { format: "rgba8unorm" }] },
      });
      const encoder = device.createCommandEncoder();
      [[undefined, attachment], new Array(1).concat([attachment])].forEach(
        (colorAttachments) => {
          const pass = encoder.beginRenderPass({ colorAttachments });
          pass.setPipeline(pipeline);
          pass.draw(1);
          pass.end();
        },
      );
      device.queue.submit([encoder.finish()]);
      const bundleEncoder = device.createRenderBundleEncoder({
        colorFormats: [undefined, "rgba8unorm"],
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

  it("accepts trailing and all-null entries", async () => {
    const result = await client.eval(({ device }) => {
      const module = device.createShaderModule({
        code: `
          @vertex fn vs() -> @builtin(position) vec4f { return vec4f(0, 0, 0, 1); }
          @fragment fn fs() -> @location(0) vec4f { return vec4f(1, 0, 0, 1); }
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
        vertex: { module, buffers: [null, null] },
        fragment: { module, targets: [{ format: "rgba8unorm" }, null] },
      });
      const encoder = device.createCommandEncoder();
      const pass = encoder.beginRenderPass({
        colorAttachments: [
          { view: target.createView(), loadOp: "clear", storeOp: "store" },
          null,
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

  it("keeps the index of a bind group layout after a null one", async () => {
    const result = await client.eval(({ device }) => {
      const module = device.createShaderModule({
        code: `
          @group(1) @binding(0) var<storage, read_write> out: u32;
          @compute @workgroup_size(1) fn main() { out = 42u; }
        `,
      });
      const layout = device.createBindGroupLayout({
        entries: [
          {
            binding: 0,
            visibility: GPUShaderStage.COMPUTE,
            buffer: { type: "storage" },
          },
        ],
      });
      const storage = device.createBuffer({
        size: 4,
        usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC,
      });
      const readback = device.createBuffer({
        size: 4,
        usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
      });
      device.pushErrorScope("validation");
      const pipeline = device.createComputePipeline({
        layout: device.createPipelineLayout({
          bindGroupLayouts: [null, layout],
        }),
        compute: { module },
      });
      const encoder = device.createCommandEncoder();
      const pass = encoder.beginComputePass();
      pass.setPipeline(pipeline);
      pass.setBindGroup(
        1,
        device.createBindGroup({
          layout,
          entries: [{ binding: 0, resource: { buffer: storage } }],
        }),
      );
      pass.dispatchWorkgroups(1);
      pass.end();
      encoder.copyBufferToBuffer(storage, 0, readback, 0, 4);
      device.queue.submit([encoder.finish()]);
      return device.popErrorScope().then((error) =>
        readback.mapAsync(GPUMapMode.READ).then(() => {
          const value = Array.from(new Uint32Array(readback.getMappedRange()));
          readback.unmap();
          return { error: error ? error.message : null, value };
        }),
      );
    });
    expect(result.error).toBeNull();
    expect(result.value).toEqual([42]);
  });

  it("treats a vertex buffer layout with no attributes as a used slot", async () => {
    const result = await client.eval(({ device }) => {
      const module = device.createShaderModule({
        code: `
          @vertex fn vs() -> @builtin(position) vec4f { return vec4f(0, 0, 0, 1); }
          @fragment fn fs() -> @location(0) vec4f { return vec4f(1, 0, 0, 1); }
        `,
      });
      const target = device.createTexture({
        size: [4, 4],
        format: "rgba8unorm",
        usage: GPUTextureUsage.RENDER_ATTACHMENT,
      });
      const pipeline = device.createRenderPipeline({
        layout: "auto",
        vertex: { module, buffers: [{ arrayStride: 16, attributes: [] }] },
        fragment: { module, targets: [{ format: "rgba8unorm" }] },
      });
      device.pushErrorScope("validation");
      const encoder = device.createCommandEncoder();
      const pass = encoder.beginRenderPass({
        colorAttachments: [
          { view: target.createView(), loadOp: "clear", storeOp: "store" },
        ],
      });
      pass.setPipeline(pipeline);
      // Slot 0 is used by the pipeline but never bound.
      pass.draw(1);
      pass.end();
      encoder.finish();
      return device
        .popErrorScope()
        .then((error) => ({ failed: error !== null }));
    });
    expect(result.failed).toBe(true);
  });
});
