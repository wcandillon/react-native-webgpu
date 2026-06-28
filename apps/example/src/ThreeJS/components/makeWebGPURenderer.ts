import * as THREE from "three";

export const makeWebGPURenderer = (
  context: GPUCanvasContext,
  { antialias = true }: { antialias?: boolean } = {},
) =>
  new THREE.WebGPURenderer({
    antialias,
    canvas: context.canvas,
    context,
  });
