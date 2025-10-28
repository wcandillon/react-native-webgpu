import { useEffect, useMemo, useRef, useState } from "react";
import { Button, PixelRatio, StyleSheet, Text, View } from "react-native";
import type { CanvasRef } from "react-native-wgpu";
import { Canvas, useDevice } from "react-native-wgpu";
import * as d from "typegpu/data";
import tgpu, { type TgpuBindGroup, type TgpuBuffer } from "typegpu";

import { vertWGSL, fragWGSL } from "./gradientWgsl";

const Span = d.struct({
  x: d.u32,
  y: d.u32,
});

const bindGroupLayout = tgpu.bindGroupLayout({
  span: { uniform: Span },
});

interface RenderingState {
  pipeline: GPURenderPipeline;
  spanBuffer: TgpuBuffer<typeof Span>;
  bindGroup: TgpuBindGroup<(typeof bindGroupLayout)["entries"]>;
}

function useRoot() {
  const { device } = useDevice();

  return useMemo(
    () => (device ? tgpu.initFromDevice({ device }) : null),
    [device],
  );
}

export function GradientTiles() {
  const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
  const [state, setState] = useState<null | RenderingState>(null);
  const [spanX, setSpanX] = useState(4);
  const [spanY, setSpanY] = useState(4);
  const root = useRoot();
  const { device = null } = root ?? {};
  const ref = useRef<CanvasRef>(null);

  useEffect(() => {
    if (!device || !root || state !== null) {
      return;
    }
    const context = ref.current?.getContext("webgpu")!;

    const canvas = context.canvas as HTMLCanvasElement;
    canvas.width = canvas.clientWidth * PixelRatio.get();
    canvas.height = canvas.clientHeight * PixelRatio.get();
    context.configure({
      device,
      format: presentationFormat,
    });

    const spanBuffer = root
      .createBuffer(Span, { x: 10, y: 10 })
      .$usage("uniform");

    const shader = device.createShaderModule({
      code: tgpu.resolve({
        template: `${vertWGSL} ${fragWGSL}`,
        externals: {
          _EXT_: {
            span: bindGroupLayout.bound.span,
          },
        },
      }),
    });

    const pipeline = device.createRenderPipeline({
      layout: device.createPipelineLayout({
        bindGroupLayouts: [root.unwrap(bindGroupLayout)],
      }),
      vertex: {
        module: shader,
      },
      fragment: {
        module: shader,
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

    const bindGroup = root.createBindGroup(bindGroupLayout, {
      span: spanBuffer,
    });

    setState({ bindGroup, pipeline, spanBuffer });
  }, [ref, device, root, presentationFormat, state]);

  useEffect(() => {
    if (!device || !root || !state) {
      return;
    }

    const { bindGroup, pipeline, spanBuffer } = state;
    const context = ref.current?.getContext("webgpu")!;
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

    spanBuffer.write({ x: spanX, y: spanY });

    const commandEncoder = device.createCommandEncoder();
    const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
    passEncoder.setPipeline(pipeline);
    passEncoder.setBindGroup(0, root.unwrap(bindGroup));
    passEncoder.draw(4);
    passEncoder.end();

    device.queue.submit([commandEncoder.finish()]);
    context.present();
  }, [ref, device, root, spanX, spanY, state]);

  return (
    <View style={style.container}>
      <Canvas ref={ref} style={style.webgpu} transparent />
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
