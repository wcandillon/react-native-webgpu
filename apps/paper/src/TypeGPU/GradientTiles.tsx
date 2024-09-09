import React from "react";
import { Button, StyleSheet, View } from "react-native";
import { Canvas } from "react-native-wgpu";
import { struct, u32 } from "typegpu/data";
import tgpu from "typegpu";

import { useWebGPU } from "../components/useWebGPU";

let span = 1;

const vertWGSL = `
struct Output {
  @builtin(position) posOut: vec4f,
  @location(0) uvOut: vec2f,
}

@vertex
fn main(
  @builtin(vertex_index) VertexIndex : u32
) -> Output {
  var pos = array<vec2f, 4>(
    vec2(1, 1), // top-right
    vec2(-1, 1), // top-left
    vec2(1, -1), // bottom-right
    vec2(-1, -1) // bottom-left
  );

  var uv = array<vec2f, 4>(
    vec2(1., 1.), // top-right
    vec2(0., 1.), // top-left
    vec2(1., 0.), // bottom-right
    vec2(0., 0.) // bottom-left
  );

  var out: Output;
  out.posOut = vec4f(pos[VertexIndex], 0.0, 1.0);
  out.uvOut = uv[VertexIndex];
  return out;
}`;

const fragWGSL = `
struct Span {
  x: u32,
  y: u32,
}

@group(0) @binding(0) var<uniform> span: Span;

@fragment
fn main(
  @location(0) uvOut: vec2f,
) -> @location(0) vec4f {
  let red = floor(uvOut.x * f32(span.x)) / f32(span.x);
  let green = floor(uvOut.y * f32(span.y)) / f32(span.y);
  return vec4(red, green, 0.5, 1.0);
}`;

export function GradientTiles() {
  const { canvasRef: ref } = useWebGPU(
    ({ context, device, presentationFormat }) => {
      const Span = struct({
        x: u32,
        y: u32,
      });

      const spanBuffer = tgpu
        .createBuffer(Span, { x: 10, y: 10 })
        .$device(device)
        .$usage(tgpu.Uniform);

      const pipeline = device.createRenderPipeline({
        layout: "auto",
        vertex: {
          module: device.createShaderModule({
            code: vertWGSL,
          }),
        },
        fragment: {
          module: device.createShaderModule({
            code: fragWGSL,
          }),
          targets: [
            {
              format: presentationFormat,
            },
          ],
        },
        primitive: {
          topology: "triangle-strip",
        },
      });

      const bindGroup = device.createBindGroup({
        layout: pipeline.getBindGroupLayout(0),
        entries: [
          {
            binding: 0,
            resource: {
              buffer: spanBuffer.buffer,
            },
          },
        ],
      });

      return () => {
        const textureView = context.getCurrentTexture().createView();

        const renderPassDescriptor: GPURenderPassDescriptor = {
          colorAttachments: [
            {
              view: textureView,
              clearValue: [0, 0, 0, 1],
              loadOp: "clear",
              storeOp: "store",
            },
          ],
        };

        tgpu.write(spanBuffer, { x: span, y: span });

        const commandEncoder = device.createCommandEncoder();
        const passEncoder =
          commandEncoder.beginRenderPass(renderPassDescriptor);
        passEncoder.setPipeline(pipeline);
        passEncoder.setBindGroup(0, bindGroup);
        passEncoder.draw(4);
        passEncoder.end();

        device.queue.submit([commandEncoder.finish()]);
      };
    }
  );

  return (
    <View style={style.container}>
      <Canvas ref={ref} style={style.webgpu} />
      <View>
        <Button
          title="-"
          onPress={() => {
            span -= 1;
          }}
        />
        <Button
          title="+"
          onPress={() => {
            span += 1;
          }}
        />
      </View>
    </View>
  );
}

const style = StyleSheet.create({
  container: {
    flex: 1,
  },
  webgpu: {
    aspectRatio: 1,
  },
});
