import { Canvas, useCanvasEffect } from "react-native-wgpu";
import { View } from "react-native";
import { runOnUI } from "react-native-reanimated";

import CubeSceneSrc from "./CubeSceneSrc";

export const Cube = () => {
  const ref = useCanvasEffect(async () => {
    const context = ref.current!.getContext("webgpu")!;
    runOnUI(async () => {
      global.setImmediate = requestAnimationFrame;

      const adapter = await navigator.gpu.requestAdapter();
      const device = await adapter!.requestDevice();
      console.log("DONE!");

      //eval(CubeSceneSrc);
      //await global.renderCubeScene(device, context);
    })();
  });

  return (
    <View style={{ flex: 1 }}>
      <Canvas ref={ref} style={{ flex: 1 }} />
    </View>
  );
};
