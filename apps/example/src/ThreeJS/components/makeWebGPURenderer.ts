import type { NativeCanvas } from "react-native-wgpu";
import * as THREE from "three";

// Here we need to wrap the Canvas into a non-host object for now
export class ReactNativeCanvas {
  constructor(private canvas: NativeCanvas) {}

  get width() {
    return this.canvas.width;
  }

  get height() {
    return this.canvas.height;
  }

  set width(width: number) {
    this.canvas.width = width;
  }

  set height(height: number) {
    this.canvas.height = height;
  }

  get clientWidth() {
    return this.canvas.width;
  }

  get clientHeight() {
    return this.canvas.height;
  }

  set clientWidth(width: number) {
    this.canvas.width = width;
  }

  set clientHeight(height: number) {
    this.canvas.height = height;
  }

  addEventListener(_type: string, _listener: EventListener) {
    // TODO
  }

  removeEventListener(_type: string, _listener: EventListener) {
    // TODO
  }

  dispatchEvent(_event: Event) {
    // TODO
  }

  setPointerCapture() {
    // TODO
  }

  releasePointerCapture() {
    // TODO
  }
}

export const makeWebGPURenderer = (
  context: GPUCanvasContext,
  {
    antialias = true,
    device,
    alpha = false,
  }: { antialias?: boolean; device?: GPUDevice; alpha?: boolean } = {},
) =>
  new THREE.WebGPURenderer({
    antialias,
    alpha,
    // eslint-disable-next-line @typescript-eslint/ban-ts-comment
    // @ts-expect-error
    canvas: new ReactNativeCanvas(context.canvas),
    context,
    // When supplied, three.js skips its own adapter/device acquisition and
    // uses this device. Lets callers request custom features (e.g. Dawn's
    // shared-texture-memory) that three.js doesn't include in its default
    // GPUFeatureName enum walk.
    ...(device ? { device } : {}),
  });
