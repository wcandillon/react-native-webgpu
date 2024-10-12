import { Canvas, useSurface } from "react-native-wgpu";

export const CanvasAPI = () => {
  const { ref, surface } = useSurface();
  if (!surface) {
    console.log("No surface");
  } else {
    console.log(
      `surface: ${surface.width}x${
        surface.height
      } (${surface.surface.toString()})`,
    );
  }
  return <Canvas style={{ flex: 1 }} ref={ref} />;
};
