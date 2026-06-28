import * as THREE from "three/webgpu";

export function makeWebGPURenderer(context: GPUCanvasContext) {
  return new THREE.WebGPURenderer({
    antialias: true,
    canvas: context.canvas,
    context,
  });
}
