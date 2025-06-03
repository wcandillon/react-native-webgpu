import { NativeCanvas, RNCanvasContext } from "./types";

//Only used on the web
export function contextIdToId(contextId: number) {
  return "rnwgpu-canvas-" + contextId;
}

export function getNativeSurface(contextId: number): NativeCanvas {
  const canvas = document.getElementById(
    contextIdToId(contextId),
  ) as HTMLCanvasElement;

  const { height, width } = canvas.getBoundingClientRect()!;

  return {
    surface: BigInt(contextId),
    height,
    width,
    clientHeight: height,
    clientWidth: width,
  };
}

export function MakeWebGPUCanvasContext(
  contextId: number,
  width: number,
  height: number,
): RNCanvasContext {
  const canvas = document.getElementById(
    contextIdToId(contextId),
  ) as HTMLCanvasElement;

  if (
    canvas.getAttribute("width") !== width.toString() ||
    canvas.getAttribute("height") !== height.toString()
  ) {
    canvas.setAttribute("width", width.toString());
    canvas.setAttribute("height", height.toString());
  }

  const context = canvas.getContext("webgpu")!;

  return Object.assign(context, {
    present: () => {},
  });
}

export function fabricIsEnabled(): boolean {
  return true;
}
