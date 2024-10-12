import { Canvas, useSurface } from "react-native-wgpu";

export const CanvasAPI = () => {
  const { ref, surface } = useSurface();
  console.log({ surface });
  return <Canvas ref={ref} />;
};
