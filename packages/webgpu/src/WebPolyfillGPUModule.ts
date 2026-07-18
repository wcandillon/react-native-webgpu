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
    // On web the real DOM canvas handles events; delegate to it.
    addEventListener: canvas.addEventListener.bind(canvas),
    removeEventListener: canvas.removeEventListener.bind(canvas),
    dispatchEvent: canvas.dispatchEvent.bind(canvas),
    setPointerCapture: canvas.setPointerCapture.bind(canvas),
    releasePointerCapture: canvas.releasePointerCapture.bind(canvas),
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
  // On web there is no manual present; expose a no-op so RNCanvasContext's
  // present() (called after queue.submit() on native) is callable here too.
  return Object.assign(context, { present: () => {} });
}

// @ts-expect-error - polyfill for RNWebGPU native module
window.RNWebGPU = {
  getNativeSurface,
  MakeWebGPUCanvasContext: makeWebGPUCanvasContext,
  fabric,
  sessionId: 0,
};
