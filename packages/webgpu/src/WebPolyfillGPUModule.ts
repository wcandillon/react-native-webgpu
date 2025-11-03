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

  const dpr = window.devicePixelRatio || 1;
  const pixelWidth = (width * dpr).toString();
  const pixelHeight = (height * dpr).toString();

  if (
    canvas.getAttribute("width") !== pixelWidth ||
    canvas.getAttribute("height") !== pixelHeight
  ) {
    canvas.setAttribute("width", pixelWidth);
    canvas.setAttribute("height", pixelHeight);
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
