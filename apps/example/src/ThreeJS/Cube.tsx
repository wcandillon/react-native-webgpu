import { Canvas, useCanvasEffect } from "react-native-wgpu";
import { View } from "react-native";
import { runOnUI, useAnimatedReaction } from "react-native-reanimated";
import { useClock } from "@shopify/react-native-skia";

import CubeSceneSrc from "./CubeSceneSrc";

const { gpu } = navigator;
const { RNWebGPU } = global;
const { GPUBufferUsage } = global;
const { GPUColorWrite } = global;
const { GPUMapMode } = global;
const { GPUShaderStage } = global;
const { GPUTextureUsage } = global;

export const Cube = () => {
  const clock = useClock();
  const ref = useCanvasEffect(async () => {
    const context = ref.current!.getContext("webgpu")!;
    const { width, height } = context.canvas;
    const adapter = await navigator.gpu.requestAdapter();
    const device = await adapter.requestDevice();
    runOnUI(() => {
      navigator.gpu = gpu;
      global.context = context;
      global.RNWebGPU = RNWebGPU;
      global.GPUBufferUsage = GPUBufferUsage;
      global.GPUColorWrite = GPUColorWrite;
      global.GPUMapMode = GPUMapMode;
      global.GPUShaderStage = GPUShaderStage;
      global.GPUTextureUsage = GPUTextureUsage;
      global.setImmediate = requestAnimationFrame;
      global.self = global;

      eval(CubeSceneSrc);
      global.renderCubeScene(context, device);
    })();
  });

  useAnimatedReaction(
    () => clock.value,
    (time) => {
      // function animate(time: number) {
      if (!global.renderer || !global.renderer._initialized) {
        return;
      }
      global.mesh.rotation.x = time / 2000;
      global.mesh.rotation.y = time / 1000;

      global.renderer.render(global.scene, global.camera);
      global.context.present();
    },
  );

  return (
    <View style={{ flex: 1 }}>
      <Canvas ref={ref} style={{ flex: 1 }} />
    </View>
  );
};
