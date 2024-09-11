import React, { useEffect, useState } from "react";
import { Button, StyleSheet, View, Text } from "react-native";
import { Canvas } from "react-native-wgpu";
import { struct, u32 } from "typegpu/data";
import tgpu from "typegpu";

import { useWebGPU } from "../components/useWebGPU";

import { vertWGSL, fragWGSL } from "./gradientWgsl";

let draw = (_: number, __: number) => {};

export function GradientTiles() {
  const [spanX, setSpanX] = useState(4);
  const [spanY, setSpanY] = useState(4);

  useEffect(() => {
    draw(spanX, spanY);
  }, [spanX, spanY]);

  const { canvasRef } = useWebGPU(({ context, device, presentationFormat }) => {
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

    draw = (spanXValue: number, spanYValue: number) => {
      const textureView = context.getCurrentTexture().createView();
      const renderPassDescriptor: GPURenderPassDescriptor = {
        colorAttachments: [
          {
            view: textureView,
            clearValue: [0, 0, 0, 0],
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      };

      tgpu.write(spanBuffer, { x: spanXValue, y: spanYValue });

      const commandEncoder = device.createCommandEncoder();
      const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
      passEncoder.setPipeline(pipeline);
      passEncoder.setBindGroup(0, bindGroup);
      passEncoder.draw(4);
      passEncoder.end();

      device.queue.submit([commandEncoder.finish()]);
      (
        context as GPUCanvasContext & {
          present: () => void;
        }
      ).present();
    };

    draw(spanX, spanY);
  });

  return (
    <View style={style.container}>
      <Canvas ref={canvasRef} style={style.webgpu} />
      <View style={style.controls}>
        <View style={style.buttonRow}>
          <Text style={style.spanText}>span x: </Text>
          <Button
            title="➖"
            onPress={() => setSpanX((prev) => Math.max(1, prev - 1))}
          />
          <Button
            title="➕"
            onPress={() => setSpanX((prev) => Math.min(prev + 1, 10))}
          />
        </View>

        <View style={style.buttonRow}>
          <Text style={style.spanText}>span y: </Text>
          <Button
            title="➖"
            onPress={() => setSpanY((prev) => Math.max(1, prev - 1))}
          />
          <Button
            title="➕"
            onPress={() => setSpanY((prev) => Math.min(prev + 1, 10))}
          />
        </View>
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
  spanText: {
    fontSize: 20,
    fontWeight: "bold",
  },
  controls: {
    flex: 1,
    justifyContent: "center",
  },
  buttonRow: {
    flexDirection: "row",
    justifyContent: "center",
    alignItems: "center",
  },
});
