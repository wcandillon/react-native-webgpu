import { checkImage, client, encodeImage } from "./setup";

describe("Video Support", () => {
  it("Video texture pipeline creation", async () => {
    const result = await client.eval(({ device, canvas, ctx }) => {
      // Create shader for video texture rendering
      const shader = device.createShaderModule({
        label: "video-texture-shader",
        code: /* wgsl */ `
          struct VertexOutput {
            @builtin(position) position: vec4<f32>,
            @location(0) texCoord: vec2<f32>,
          }

          @vertex
          fn vertexMain(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
            let positions = array(
              vec2(-1.0, -1.0), // bottom-left
              vec2( 1.0, -1.0), // bottom-right
              vec2(-1.0,  1.0), // top-left
              vec2( 1.0,  1.0), // top-right
            );
            
            let texCoords = array(
              vec2(0.0, 1.0), // bottom-left (flipped Y)
              vec2(1.0, 1.0), // bottom-right (flipped Y)
              vec2(0.0, 0.0), // top-left (flipped Y)
              vec2(1.0, 0.0), // top-right (flipped Y)
            );

            var output: VertexOutput;
            output.position = vec4(positions[vertexIndex], 0.0, 1.0);
            output.texCoord = texCoords[vertexIndex];
            return output;
          }

          @group(0) @binding(0) var videoSampler: sampler;
          @group(0) @binding(1) var videoTexture: texture_2d<f32>;

          @fragment
          fn fragmentMain(input: VertexOutput) -> @location(0) vec4<f32> {
            let color = textureSample(videoTexture, videoSampler, input.texCoord);
            // Add subtle film grain effect
            let grain = sin(input.texCoord.x * 1000.0) * sin(input.texCoord.y * 1000.0) * 0.02;
            return vec4(color.rgb + grain, color.a);
          }
        `,
      });

      // Create render pipeline
      const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
      const pipeline = device.createRenderPipeline({
        label: "video-texture-pipeline",
        layout: "auto",
        vertex: {
          module: shader,
          entryPoint: "vertexMain",
        },
        fragment: {
          module: shader,
          entryPoint: "fragmentMain",
          targets: [{ format: presentationFormat }],
        },
        primitive: {
          topology: "triangle-strip",
        },
      });

      // Create sampler for video texture
      const sampler = device.createSampler({
        label: "video-sampler",
        magFilter: "linear",
        minFilter: "linear",
        addressModeU: "clamp-to-edge",
        addressModeV: "clamp-to-edge",
      });

      // Create a test texture to simulate video frames
      const textureSize = 256;
      const texture = device.createTexture({
        label: "test-video-texture",
        size: [textureSize, textureSize],
        format: "rgba8unorm",
        usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST,
      });

      // Generate test pattern data
      const pixelData = new Uint8Array(textureSize * textureSize * 4);
      for (let y = 0; y < textureSize; y++) {
        for (let x = 0; x < textureSize; x++) {
          const index = (y * textureSize + x) * 4;
          const u = x / textureSize;
          const v = y / textureSize;
          
          // Create animated test pattern
          const wave1 = Math.sin(u * 10) * 0.5 + 0.5;
          const wave2 = Math.sin(v * 10) * 0.5 + 0.5;
          const spiral = Math.sin(Math.sqrt((u - 0.5) ** 2 + (v - 0.5) ** 2) * 20) * 0.5 + 0.5;
          
          pixelData[index] = Math.floor(wave1 * 255);     // R
          pixelData[index + 1] = Math.floor(wave2 * 255); // G
          pixelData[index + 2] = Math.floor(spiral * 255); // B
          pixelData[index + 3] = 255;                      // A
        }
      }

      // Write texture data
      device.queue.writeTexture(
        { texture },
        pixelData,
        { bytesPerRow: textureSize * 4 },
        [textureSize, textureSize]
      );

      // Create bind group
      const bindGroup = device.createBindGroup({
        label: "video-bind-group",
        layout: pipeline.getBindGroupLayout(0),
        entries: [
          { binding: 0, resource: sampler },
          { binding: 1, resource: texture.createView() },
        ],
      });

      // Render
      const commandEncoder = device.createCommandEncoder({ label: "video-render" });
      const textureView = ctx.getCurrentTexture().createView();

      const renderPass = commandEncoder.beginRenderPass({
        label: "video-render-pass",
        colorAttachments: [
          {
            view: textureView,
            clearValue: { r: 0.0, g: 0.0, b: 0.0, a: 1.0 },
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      });

      renderPass.setPipeline(pipeline);
      renderPass.setBindGroup(0, bindGroup);
      renderPass.draw(4); // Draw full-screen quad
      renderPass.end();

      device.queue.submit([commandEncoder.finish()]);

      return {
        pipelineCreated: !!pipeline,
        samplerCreated: !!sampler,
        textureCreated: !!texture,
        bindGroupCreated: !!bindGroup,
        image: canvas.getImageData(),
      };
    });

    // Verify that all components were created successfully
    expect(result.pipelineCreated).toBe(true);
    expect(result.samplerCreated).toBe(true);
    expect(result.textureCreated).toBe(true);
    expect(result.bindGroupCreated).toBe(true);

    // Check that the rendered image contains the expected pattern
    const image = encodeImage(result.image);
    checkImage(image, "snapshots/video-texture-test.png");
  });

  it("Video texture updates", async () => {
    const result = await client.eval(({ device }) => {
      // Test texture creation and update cycle
      const textureSize = 128;
      const texture = device.createTexture({
        label: "video-texture-update-test",
        size: [textureSize, textureSize],
        format: "rgba8unorm",
        usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST,
      });

      // Simulate multiple frame updates
      const frames = [];
      for (let frame = 0; frame < 3; frame++) {
        const pixelData = new Uint8Array(textureSize * textureSize * 4);
        const time = frame * 0.1;
        
        for (let y = 0; y < textureSize; y++) {
          for (let x = 0; x < textureSize; x++) {
            const index = (y * textureSize + x) * 4;
            const u = x / textureSize;
            const v = y / textureSize;
            
            // Animated pattern that changes over time
            const wave = Math.sin(u * 8 + time * 5) * Math.sin(v * 8 + time * 3) * 0.5 + 0.5;
            
            pixelData[index] = Math.floor(wave * 255 * (frame + 1) / 3);     // R increases per frame
            pixelData[index + 1] = Math.floor(wave * 255);                   // G
            pixelData[index + 2] = Math.floor((1 - wave) * 255);            // B
            pixelData[index + 3] = 255;                                      // A
          }
        }

        // Update texture
        device.queue.writeTexture(
          { texture },
          pixelData,
          { bytesPerRow: textureSize * 4 },
          [textureSize, textureSize]
        );

        frames.push(`frame-${frame}-updated`);
      }

      texture.destroy();

      return {
        textureCreated: !!texture,
        framesUpdated: frames.length,
        framesList: frames,
      };
    });

    expect(result.textureCreated).toBe(true);
    expect(result.framesUpdated).toBe(3);
    expect(result.framesList).toEqual([
      "frame-0-updated",
      "frame-1-updated", 
      "frame-2-updated"
    ]);
  });
});