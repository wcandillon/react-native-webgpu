import { useEffect, useState } from "react";
import { Button, PixelRatio, StyleSheet, Text, View } from "react-native";
import { Canvas, useDevice, useGPUContext } from "react-native-wgpu";
import { struct, u32 } from "typegpu/data";
import tgpu from "typegpu";

import { vertWGSL, fragWGSL } from "./gradientWgsl";

interface RenderingState {
  pipeline: GPURenderPipeline;
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  spanBuffer: any;
  bindGroup: GPUBindGroup;
}

export function GradientTiles() {
  const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
  const [state, setState] = useState<null | RenderingState>(null);
  const [spanX, setSpanX] = useState(4);
  const [spanY, setSpanY] = useState(4);
  const { device } = useDevice();
  const { ref, context } = useGPUContext();
  useEffect(() => {
    if (!device || !context || state !== null) {
      return;
    }
    const canvas = context.canvas as HTMLCanvasElement;
    canvas.width = canvas.clientWidth * PixelRatio.get();
    canvas.height = canvas.clientHeight * PixelRatio.get();
    context.configure({
      device,
      format: presentationFormat,
    });

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
    setState({ bindGroup, pipeline, spanBuffer });
  }, [context, device, presentationFormat, state]);

  useEffect(() => {
    if (!context || !device || !state) {
      return;
    }
    const { bindGroup, pipeline, spanBuffer } = state;
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

    tgpu.write(spanBuffer, { x: spanX, y: spanY });

    const commandEncoder = device.createCommandEncoder();
    const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
    passEncoder.setPipeline(pipeline);
    passEncoder.setBindGroup(0, bindGroup);
    passEncoder.draw(4);
    passEncoder.end();

    device.queue.submit([commandEncoder.finish()]);
    context.present();
  }, [context, device, spanX, spanY, state]);

  return (
    <View style={style.container}>
      <Canvas ref={ref} style={style.webgpu} />
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
