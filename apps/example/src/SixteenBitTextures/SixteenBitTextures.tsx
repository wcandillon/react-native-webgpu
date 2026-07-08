import React, { useEffect, useRef } from "react";
import { PixelRatio, StyleSheet, Text, View } from "react-native";
import type { CanvasRef } from "react-native-webgpu";
import { Canvas } from "react-native-webgpu";

// A deep-blue vertical gradient (the classic "night sky" worst case for
// banding) that slowly drifts up and down. Each channel has a different
// slope, so the channels quantize at different positions and produce both
// lightness and hue contours. On an 8-bit surface the bands crawl across the
// screen; on a 16-bit float surface the gradient stays smooth. Both canvases
// must show the same colors.
const gradientWGSL = /* wgsl */ `
struct Uniforms {
  time: f32,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

struct VertexOutput {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};

@vertex
fn vs(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
  const pos = array<vec2f, 3>(
    vec2f(-1.0, -1.0),
    vec2f(3.0, -1.0),
    vec2f(-1.0, 3.0)
  );
  var out: VertexOutput;
  out.position = vec4f(pos[vertexIndex], 0.0, 1.0);
  out.uv = pos[vertexIndex] * 0.5 + 0.5;
  return out;
}

const colorA = vec3f(0.05, 0.08, 0.20);
const colorB = vec3f(0.09, 0.14, 0.33);

@fragment
fn fs(in: VertexOutput) -> @location(0) vec4f {
  let phase = 0.15 * sin(uniforms.time * 0.5);
  let t = clamp(in.uv.y + phase, 0.0, 1.0);
  let color = mix(colorA, colorB, t);
  return vec4f(color, 1.0);
}
`;

const makeRenderer = (
  device: GPUDevice,
  ref: React.RefObject<CanvasRef | null>,
  format: GPUTextureFormat,
  uniformBuffer: GPUBuffer,
) => {
  const context = ref.current!.getContext("webgpu")!;
  const canvas = context.canvas as HTMLCanvasElement;
  canvas.width = canvas.clientWidth * PixelRatio.get();
  canvas.height = canvas.clientHeight * PixelRatio.get();

  context.configure({
    device,
    format,
    alphaMode: "opaque",
  });

  const module = device.createShaderModule({ code: gradientWGSL });
  const pipeline = device.createRenderPipeline({
    layout: "auto",
    vertex: {
      module,
      entryPoint: "vs",
    },
    fragment: {
      module,
      entryPoint: "fs",
      targets: [{ format }],
    },
    primitive: {
      topology: "triangle-list",
    },
  });
  const bindGroup = device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: [{ binding: 0, resource: { buffer: uniformBuffer } }],
  });

  return () => {
    const commandEncoder = device.createCommandEncoder();
    const passEncoder = commandEncoder.beginRenderPass({
      colorAttachments: [
        {
          view: context.getCurrentTexture().createView(),
          clearValue: [0, 0, 0, 1],
          loadOp: "clear",
          storeOp: "store",
        },
      ],
    });
    passEncoder.setPipeline(pipeline);
    passEncoder.setBindGroup(0, bindGroup);
    passEncoder.draw(3);
    passEncoder.end();

    device.queue.submit([commandEncoder.finish()]);
    context.present();
  };
};

export function SixteenBitTextures() {
  const ref8bit = useRef<CanvasRef>(null);
  const ref16bit = useRef<CanvasRef>(null);
  const frameId = useRef<number | null>(null);
  useEffect(() => {
    (async () => {
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter) {
        throw new Error("No adapter");
      }
      const device = await adapter.requestDevice();

      const uniformBuffer = device.createBuffer({
        size: 16,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
      });
      const draw8bit = makeRenderer(
        device,
        ref8bit,
        navigator.gpu.getPreferredCanvasFormat(),
        uniformBuffer,
      );
      const draw16bit = makeRenderer(
        device,
        ref16bit,
        "rgba16float",
        uniformBuffer,
      );

      const render = () => {
        const time = performance.now() / 1000;
        device.queue.writeBuffer(uniformBuffer, 0, new Float32Array([time]));
        draw8bit();
        draw16bit();
        frameId.current = requestAnimationFrame(render);
      };
      frameId.current = requestAnimationFrame(render);
    })();
    return () => {
      if (frameId.current !== null) {
        cancelAnimationFrame(frameId.current);
      }
    };
  }, []);

  return (
    <View style={style.container}>
      <View style={style.column}>
        <Canvas ref={ref8bit} style={style.webgpu} transparent={false} />
        <Text style={style.label}>8-bit</Text>
      </View>
      <View style={style.column}>
        <Canvas ref={ref16bit} style={style.webgpu} transparent={false} />
        <Text style={style.label}>16-bit</Text>
      </View>
    </View>
  );
}

const style = StyleSheet.create({
  container: {
    flex: 1,
    flexDirection: "row",
  },
  column: {
    flex: 1,
  },
  webgpu: {
    flex: 1,
  },
  label: {
    textAlign: "center",
    padding: 8,
    fontWeight: "bold",
  },
});
