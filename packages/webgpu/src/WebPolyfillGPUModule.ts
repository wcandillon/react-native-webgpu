import { contextIdToId } from "./utils";

const fabric = true;

function getNativeSurface(contextId: number) {
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

function makeWebGPUCanvasContext(
  contextId: number,
  width: number,
  height: number,
) {
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

// @ts-expect-error - polyfill for RNWebGPU native module
window.RNWebGPU = {
  getNativeSurface,
  MakeWebGPUCanvasContext: makeWebGPUCanvasContext,
  fabric,
};
