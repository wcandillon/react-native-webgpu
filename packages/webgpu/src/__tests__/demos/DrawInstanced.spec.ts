import { checkImage, client, encodeImage } from "../setup";

// Draws one small triangle per instance, laid out on a 3x3 grid using
// @builtin(instance_index), each with a distinct flat color.
const instancedTriangle = /*wgsl*/ `struct VertexOutput {
  @builtin(position) position: vec4f,
  @location(0) @interpolate(flat) instance: u32,
};

@vertex
fn vs(
  @builtin(vertex_index) vertexIndex: u32,
  @builtin(instance_index) instanceIndex: u32
) -> VertexOutput {
  let pos = array(
    vec2f( 0.0,   0.25),
    vec2f(-0.25, -0.25),
    vec2f( 0.25, -0.25)
  );

  // Cell centers at -2/3, 0 and 2/3 in clip space.
  let col = f32(instanceIndex % 3u);
  let row = f32(instanceIndex / 3u);
  let center = vec2f(
    -2.0 / 3.0 + col * (2.0 / 3.0),
     2.0 / 3.0 - row * (2.0 / 3.0)
  );

  var output: VertexOutput;
  output.position = vec4f(pos[vertexIndex] * 0.9 + center, 0.0, 1.0);
  output.instance = instanceIndex;
  return output;
}

@fragment
fn fs(@location(0) @interpolate(flat) instance: u32) -> @location(0) vec4f {
  let colors = array<vec3f, 9>(
    vec3f(1.0, 0.0, 0.0),
    vec3f(0.0, 1.0, 0.0),
    vec3f(0.0, 0.0, 1.0),
    vec3f(1.0, 1.0, 0.0),
    vec3f(1.0, 0.0, 1.0),
    vec3f(0.0, 1.0, 1.0),
    vec3f(1.0, 0.5, 0.0),
    vec3f(0.5, 0.0, 1.0),
    vec3f(1.0, 1.0, 1.0)
  );
  return vec4f(colors[instance % 9u], 1.0);
}
`;

// Same grid layout but the geometry is a quad rendered through an index
// buffer, so drawIndexed() gets exercised with an instance count.
const instancedQuad = /*wgsl*/ `struct VertexOutput {
  @builtin(position) position: vec4f,
  @location(0) @interpolate(flat) instance: u32,
};

@vertex
fn vs(
  @builtin(vertex_index) vertexIndex: u32,
  @builtin(instance_index) instanceIndex: u32
) -> VertexOutput {
  let pos = array(
    vec2f(-0.2,  0.2),
    vec2f(-0.2, -0.2),
    vec2f( 0.2, -0.2),
    vec2f( 0.2,  0.2)
  );

  let col = f32(instanceIndex % 3u);
  let row = f32(instanceIndex / 3u);
  let center = vec2f(
    -2.0 / 3.0 + col * (2.0 / 3.0),
     2.0 / 3.0 - row * (2.0 / 3.0)
  );

  var output: VertexOutput;
  output.position = vec4f(pos[vertexIndex] + center, 0.0, 1.0);
  output.instance = instanceIndex;
  return output;
}

@fragment
fn fs(@location(0) @interpolate(flat) instance: u32) -> @location(0) vec4f {
  let colors = array<vec3f, 9>(
    vec3f(1.0, 0.0, 0.0),
    vec3f(0.0, 1.0, 0.0),
    vec3f(0.0, 0.0, 1.0),
    vec3f(1.0, 1.0, 0.0),
    vec3f(1.0, 0.0, 1.0),
    vec3f(0.0, 1.0, 1.0),
    vec3f(1.0, 0.5, 0.0),
    vec3f(0.5, 0.0, 1.0),
    vec3f(1.0, 1.0, 1.0)
  );
  return vec4f(colors[instance % 9u], 1.0);
}
`;

// Per-instance data comes from a stepMode: "instance" vertex buffer instead
// of @builtin(instance_index).
const instanceStepMode = /*wgsl*/ `struct VertexOutput {
  @builtin(position) position: vec4f,
  @location(0) color: vec4f,
};

@vertex
fn vs(
  @location(0) position: vec2f,
  @location(1) offset: vec2f,
  @location(2) color: vec4f
) -> VertexOutput {
  var output: VertexOutput;
  output.position = vec4f(position + offset, 0.0, 1.0);
  output.color = color;
  return output;
}

@fragment
fn fs(@location(0) color: vec4f) -> @location(0) vec4f {
  return color;
}
`;

describe("DrawInstanced", () => {
  it("draw with an instance count", async () => {
    const result = await client.eval(
      ({ device, gpu, ctx, canvas, instancedTriangleWGSL }) => {
        const module = device.createShaderModule({
          code: instancedTriangleWGSL,
        });
        const pipeline = device.createRenderPipeline({
          layout: "auto",
          vertex: { module },
          fragment: {
            module,
            targets: [{ format: gpu.getPreferredCanvasFormat() }],
          },
          primitive: { topology: "triangle-list" },
        });

        const commandEncoder = device.createCommandEncoder();
        const passEncoder = commandEncoder.beginRenderPass({
          colorAttachments: [
            {
              view: ctx.getCurrentTexture().createView(),
              clearValue: [0, 0, 0, 1],
              loadOp: "clear",
              storeOp: "store",
            },
          ],
        });
        passEncoder.setPipeline(pipeline);
        passEncoder.draw(3, 9);
        passEncoder.end();
        device.queue.submit([commandEncoder.finish()]);
        return canvas.getImageData();
      },
      { instancedTriangleWGSL: instancedTriangle },
    );
    const image = encodeImage(result);
    checkImage(image, "snapshots/draw-instanced.png");
  });
  it("draw with a non-zero firstInstance", async () => {
    const result = await client.eval(
      ({ device, gpu, ctx, canvas, instancedTriangleWGSL }) => {
        const module = device.createShaderModule({
          code: instancedTriangleWGSL,
        });
        const pipeline = device.createRenderPipeline({
          layout: "auto",
          vertex: { module },
          fragment: {
            module,
            targets: [{ format: gpu.getPreferredCanvasFormat() }],
          },
          primitive: { topology: "triangle-list" },
        });

        const commandEncoder = device.createCommandEncoder();
        const passEncoder = commandEncoder.beginRenderPass({
          colorAttachments: [
            {
              view: ctx.getCurrentTexture().createView(),
              clearValue: [0, 0, 0, 1],
              loadOp: "clear",
              storeOp: "store",
            },
          ],
        });
        passEncoder.setPipeline(pipeline);
        // instance_index starts at firstInstance, so only the two bottom
        // rows of the grid (instances 3 to 8) are drawn.
        passEncoder.draw(3, 6, 0, 3);
        passEncoder.end();
        device.pushErrorScope("validation");
        device.queue.submit([commandEncoder.finish()]);
        return device.popErrorScope().then((error) => {
          if (error) {
            throw new Error(
              `firstInstance draw failed validation: ${error.message}`,
            );
          }
          return canvas.getImageData();
        });
      },
      { instancedTriangleWGSL: instancedTriangle },
    );
    // The clear alone makes every pixel opaque black, so an all-zero readback
    // means the whole submit was discarded — the iOS Simulator failure mode
    // for base-instance draws — rather than a rendering difference. Guard
    // before checkImage so a blank image is never written as the reference.
    expect(result.data.some((value) => value !== 0)).toBe(true);
    const image = encodeImage(result);
    checkImage(image, "snapshots/draw-instanced-first-instance.png");
  });
  it("drawIndexed with an instance count", async () => {
    const result = await client.eval(
      ({ device, gpu, ctx, canvas, instancedQuadWGSL }) => {
        const module = device.createShaderModule({
          code: instancedQuadWGSL,
        });
        const pipeline = device.createRenderPipeline({
          layout: "auto",
          vertex: { module },
          fragment: {
            module,
            targets: [{ format: gpu.getPreferredCanvasFormat() }],
          },
          primitive: { topology: "triangle-list" },
        });

        const indices = new Uint16Array([0, 1, 2, 0, 2, 3]);
        const indexBuffer = device.createBuffer({
          size: indices.byteLength,
          usage: GPUBufferUsage.INDEX,
          mappedAtCreation: true,
        });
        new Uint16Array(indexBuffer.getMappedRange()).set(indices);
        indexBuffer.unmap();

        const commandEncoder = device.createCommandEncoder();
        const passEncoder = commandEncoder.beginRenderPass({
          colorAttachments: [
            {
              view: ctx.getCurrentTexture().createView(),
              clearValue: [0, 0, 0, 1],
              loadOp: "clear",
              storeOp: "store",
            },
          ],
        });
        passEncoder.setPipeline(pipeline);
        passEncoder.setIndexBuffer(indexBuffer, "uint16");
        passEncoder.drawIndexed(6, 9);
        passEncoder.end();
        device.queue.submit([commandEncoder.finish()]);
        return canvas.getImageData();
      },
      { instancedQuadWGSL: instancedQuad },
    );
    const image = encodeImage(result);
    checkImage(image, "snapshots/draw-indexed-instanced.png");
  });
  it("draw with a stepMode instance vertex buffer", async () => {
    const result = await client.eval(
      ({ device, gpu, ctx, canvas, instanceStepModeWGSL }) => {
        const module = device.createShaderModule({
          code: instanceStepModeWGSL,
        });
        const pipeline = device.createRenderPipeline({
          layout: "auto",
          vertex: {
            module,
            buffers: [
              {
                arrayStride: 2 * 4,
                stepMode: "vertex",
                attributes: [
                  { shaderLocation: 0, offset: 0, format: "float32x2" },
                ],
              },
              {
                arrayStride: 6 * 4,
                stepMode: "instance",
                attributes: [
                  { shaderLocation: 1, offset: 0, format: "float32x2" },
                  { shaderLocation: 2, offset: 2 * 4, format: "float32x4" },
                ],
              },
            ],
          },
          fragment: {
            module,
            targets: [{ format: gpu.getPreferredCanvasFormat() }],
          },
          primitive: { topology: "triangle-list" },
        });

        // One triangle, advanced per vertex.
        const vertices = new Float32Array([0, 0.3, -0.3, -0.3, 0.3, -0.3]);
        const vertexBuffer = device.createBuffer({
          size: vertices.byteLength,
          usage: GPUBufferUsage.VERTEX,
          mappedAtCreation: true,
        });
        new Float32Array(vertexBuffer.getMappedRange()).set(vertices);
        vertexBuffer.unmap();

        // Offset (vec2) and color (vec4), advanced per instance.
        // prettier-ignore
        const instances = new Float32Array([
          -0.5,  0.5, 1, 0, 0, 1,
           0.5,  0.5, 0, 1, 0, 1,
          -0.5, -0.5, 0, 0, 1, 1,
           0.5, -0.5, 1, 1, 0, 1,
        ]);
        const instanceBuffer = device.createBuffer({
          size: instances.byteLength,
          usage: GPUBufferUsage.VERTEX,
          mappedAtCreation: true,
        });
        new Float32Array(instanceBuffer.getMappedRange()).set(instances);
        instanceBuffer.unmap();

        const commandEncoder = device.createCommandEncoder();
        const passEncoder = commandEncoder.beginRenderPass({
          colorAttachments: [
            {
              view: ctx.getCurrentTexture().createView(),
              clearValue: [0, 0, 0, 1],
              loadOp: "clear",
              storeOp: "store",
            },
          ],
        });
        passEncoder.setPipeline(pipeline);
        passEncoder.setVertexBuffer(0, vertexBuffer);
        passEncoder.setVertexBuffer(1, instanceBuffer);
        passEncoder.draw(3, 4);
        passEncoder.end();
        device.queue.submit([commandEncoder.finish()]);
        return canvas.getImageData();
      },
      { instanceStepModeWGSL: instanceStepMode },
    );
    const image = encodeImage(result);
    checkImage(image, "snapshots/draw-instanced-step-mode.png");
  });
  it("drawIndirect with an instance count", async () => {
    const result = await client.eval(
      ({ device, gpu, ctx, canvas, instancedTriangleWGSL }) => {
        const module = device.createShaderModule({
          code: instancedTriangleWGSL,
        });
        const pipeline = device.createRenderPipeline({
          layout: "auto",
          vertex: { module },
          fragment: {
            module,
            targets: [{ format: gpu.getPreferredCanvasFormat() }],
          },
          primitive: { topology: "triangle-list" },
        });

        // vertexCount, instanceCount, firstVertex, firstInstance. A non-zero
        // firstInstance would require the indirect-first-instance feature.
        const indirectParams = new Uint32Array([3, 9, 0, 0]);
        const indirectBuffer = device.createBuffer({
          size: indirectParams.byteLength,
          usage: GPUBufferUsage.INDIRECT,
          mappedAtCreation: true,
        });
        new Uint32Array(indirectBuffer.getMappedRange()).set(indirectParams);
        indirectBuffer.unmap();

        const commandEncoder = device.createCommandEncoder();
        const passEncoder = commandEncoder.beginRenderPass({
          colorAttachments: [
            {
              view: ctx.getCurrentTexture().createView(),
              clearValue: [0, 0, 0, 1],
              loadOp: "clear",
              storeOp: "store",
            },
          ],
        });
        passEncoder.setPipeline(pipeline);
        passEncoder.drawIndirect(indirectBuffer, 0);
        passEncoder.end();
        device.queue.submit([commandEncoder.finish()]);
        return canvas.getImageData();
      },
      { instancedTriangleWGSL: instancedTriangle },
    );
    const image = encodeImage(result);
    // An indirect draw must produce the same image as the equivalent
    // direct draw(3, 9) above.
    checkImage(image, "snapshots/draw-instanced.png");
  });
});
