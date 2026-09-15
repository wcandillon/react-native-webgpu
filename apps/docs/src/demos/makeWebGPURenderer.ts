import * as THREE from "three/webgpu";

export function makeWebGPURenderer(
  context: GPUCanvasContext,
  device?: GPUDevice,
) {
  return new THREE.WebGPURenderer({
    antialias: true,
    canvas: context.canvas,
    context,
    device,
  });
}
