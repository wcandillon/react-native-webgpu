import { useEffect, useState } from "react";
import { PixelRatio } from "react-native";
import { Canvas, useGPUContext } from "react-native-wgpu";

interface TextureState {
  pipeline: GPURenderPipeline;
  sampler: GPUSampler;
}

interface GPUTextureProps {
  texture: GPUTexture | null;
  device: GPUDevice | null;
  style?: {
    width: number;
    height: number;
  };
}

export const Texture = ({ texture, style, device }: GPUTextureProps) => {
  const [state, setState] = useState<TextureState | null>(null);
  const { ref, context } = useGPUContext();
  useEffect(() => {
    if (!device || !context) {
      return;
    }

    const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
    const canvas = context.canvas as HTMLCanvasElement;
    canvas.width = canvas.clientWidth * PixelRatio.get();
    canvas.height = canvas.clientHeight * PixelRatio.get();

    context.configure({
      device,
      format: presentationFormat,
      alphaMode: "premultiplied",
    });

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
    setState({
      pipeline,
      sampler,
    });
  }, [context, device]);
  useEffect(() => {
    if (!texture || !state || !device || !context) {
      return;
    }
    const { pipeline, sampler } = state;

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
  }, [device, state, texture, context]);
  return <Canvas ref={ref} style={style} />;
};
