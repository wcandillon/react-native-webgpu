import { Canvas, useCanvasEffect } from "react-native-wgpu";
import { View } from "react-native";
import { runOnUI } from "react-native-reanimated";

import CubeSceneSrc from "./CubeSceneSrc";

export const Cube = () => {
  const ref = useCanvasEffect(async () => {
    const context = ref.current!.getContext("webgpu")!;
    runOnUI(async () => {
      const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
      // global.setImmediate = requestAnimationFrame;

      console.log("requiring adapter");
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter) {
        throw new Error("No adapter");
      }
      console.log("requiring adapter");
      const device = await adapter.requestDevice();
      ref.current?.getContext("webgpu")?.configure({
        device,
        format: presentationFormat,
        alphaMode: "premultiplied",
      });
      console.log("GOT A DEVICE!");
      eval(CubeSceneSrc);
      await global.renderCubeScene(context, device);
      console.log("DONE!");
    })();
  });

  return (
    <View style={{ flex: 1 }}>
      <Canvas ref={ref} style={{ flex: 1 }} />
    </View>
  );
};
