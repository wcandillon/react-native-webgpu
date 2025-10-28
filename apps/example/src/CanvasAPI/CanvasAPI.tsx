import { Button, View } from "react-native";
import type { CanvasRef } from "react-native-wgpu";
import { Canvas } from "react-native-wgpu";
import { useRef } from "react";

import { redFragWGSL, triangleVertWGSL } from "../Triangle/triangle";

export const CanvasAPI = () => {
  const ref = useRef<CanvasRef>(null);
  return (
    <View style={{ flex: 1 }}>
      <Button
        onPress={() =>
          (async () => {
            const context = ref.current?.getContext("webgpu");
            if (!context) {
              throw new Error("No context");
            }
            const adapter = await navigator.gpu.requestAdapter();
            if (!adapter) {
              throw new Error("No adapter");
            }
            const device = await adapter.requestDevice();
            const presentationFormat = navigator.gpu.getPreferredCanvasFormat();

            const { canvas } = context;
            if (!context) {
              throw new Error("No context");
            }
            context.configure({
              device,
              format: presentationFormat,
              alphaMode: "premultiplied",
            });

            const sampleCount = 4;

            const pipeline = device.createRenderPipeline({
              layout: "auto",
              vertex: {
                module: device.createShaderModule({
                  code: triangleVertWGSL,
                }),
              },
              fragment: {
                module: device.createShaderModule({
                  code: redFragWGSL,
                }),
                targets: [
                  {
                    format: presentationFormat,
                  },
                ],
              },
              primitive: {
                topology: "triangle-list",
              },
              multisample: {
                count: sampleCount,
              },
            });

            const texture = device.createTexture({
              size: [canvas.width, canvas.height],
              sampleCount,
              format: presentationFormat,
              usage: GPUTextureUsage.RENDER_ATTACHMENT,
            });
            const view = texture.createView();

            const commandEncoder = device.createCommandEncoder();

            const renderPassDescriptor: GPURenderPassDescriptor = {
              colorAttachments: [
                {
                  view,
                  resolveTarget: context.getCurrentTexture().createView(),
                  clearValue: [0, 0, 0, 1],
                  loadOp: "clear",
                  storeOp: "discard",
                },
              ],
            };

            const passEncoder =
              commandEncoder.beginRenderPass(renderPassDescriptor);
            passEncoder.setPipeline(pipeline);
            passEncoder.draw(3);
            passEncoder.end();

            device.queue.submit([commandEncoder.finish()]);

            context.present();
          })()
        }
        title="check surface"
      />
      <Canvas style={{ flex: 1 }} ref={ref} />
    </View>
  );
};
