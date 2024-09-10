import { PixelRatio } from "react-native";
import { Canvas, useCanvasEffect } from "react-native-wgpu";

export interface Bitmap {
  width: number;
  height: number;
  colorType: "rgba8unorm" | "bgra8unorm";
  data: Uint8Array;
}

interface BitmapProps {
  bitmap: Bitmap | null;
  style?: {
    width: number;
    height: number;
  };
}

export const Bitmap = ({ bitmap, style }: BitmapProps) => {
  const ref = useCanvasEffect(async () => {
    if (!bitmap) {
      return;
    }
    const adapter = await navigator.gpu.requestAdapter();
    if (!adapter) {
      throw new Error("No adapter");
    }
    const device = await adapter.requestDevice();
    const presentationFormat = navigator.gpu.getPreferredCanvasFormat();

    const context = ref.current!.getContext("webgpu")!;
    const canvas = context.canvas as HTMLCanvasElement;
    canvas.width = canvas.clientWidth * PixelRatio.get();
    canvas.height = canvas.clientHeight * PixelRatio.get();

    context.configure({
      device,
      format: presentationFormat,
      alphaMode: "premultiplied",
    });

    // Create a texture to hold the bitmap
    const texture = device.createTexture({
      size: { width: bitmap.width, height: bitmap.height },
      format: bitmap.colorType,
      usage:
        GPUTextureUsage.TEXTURE_BINDING |
        GPUTextureUsage.COPY_DST |
        GPUTextureUsage.RENDER_ATTACHMENT,
    });

    // Copy the buffer data to the texture
    device.queue.writeTexture(
      { texture },
      bitmap.data,
      { bytesPerRow: bitmap.width * 4 },
      { width: bitmap.width, height: bitmap.height },
    );

    // Create a sampler
    const sampler = device.createSampler({
      magFilter: "linear",
      minFilter: "linear",
    });

    // Create the render pipeline
    const shader = device.createShaderModule({
      code: `
        struct VertexOutput {
          @builtin(position) position: vec4f,
          @location(0) texCoord: vec2f,
        }

        @vertex
        fn vertexMain(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
          var pos = array<vec2f, 4>(
            vec2f(-1.0, 1.0),
            vec2f(-1.0, -1.0),
            vec2f(1.0, 1.0),
            vec2f(1.0, -1.0)
          );
          var texCoord = array<vec2f, 4>(
            vec2f(0.0, 0.0),
            vec2f(0.0, 1.0),
            vec2f(1.0, 0.0),
            vec2f(1.0, 1.0)
          );
          var output: VertexOutput;
          output.position = vec4f(pos[vertexIndex], 0.0, 1.0);
          output.texCoord = texCoord[vertexIndex];
          return output;
        }

        @group(0) @binding(0) var texture: texture_2d<f32>;
        @group(0) @binding(1) var mySampler: sampler;

        @fragment
        fn fragmentMain(@location(0) texCoord: vec2f) -> @location(0) vec4f {
          return textureSample(texture, mySampler, texCoord);
        }
      `,
    });

    const pipeline = device.createRenderPipeline({
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
        stripIndexFormat: "uint32",
      },
    });

    // Create a bind group
    const bindGroup = device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: [
        {
          binding: 0,
          resource: texture.createView(),
        },
        {
          binding: 1,
          resource: sampler,
        },
      ],
    });

    // Render function
    function render() {
      const commandEncoder = device.createCommandEncoder();
      const textureView = context.getCurrentTexture().createView();

      const renderPass = commandEncoder.beginRenderPass({
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
      renderPass.draw(4);
      renderPass.end();

      device.queue.submit([commandEncoder.finish()]);
      context.present();
    }

    // Initial render
    render();
  }, [bitmap]);
  return <Canvas ref={ref} style={style} />;
};
