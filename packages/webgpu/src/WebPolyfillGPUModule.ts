import { contextIdToId } from "./utils";

const fabric = true;

function getNativeSurface(contextId: number) {
  const canvas = document.getElementById(
    contextIdToId(contextId),
  ) as HTMLCanvasElement;

  // The layout size; getBoundingClientRect() would include CSS transforms.
  const { clientHeight: height, clientWidth: width } = canvas;

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

// Sized from the canvas element's own layout size (clientWidth/clientHeight,
// which exclude CSS transforms), not from the size Canvas passes in.
function makeWebGPUCanvasContext(contextId: number) {
  const canvas = document.getElementById(
    contextIdToId(contextId),
  ) as HTMLCanvasElement;

  const dpr = window.devicePixelRatio || 1;
  const pixelWidth = (canvas.clientWidth * dpr).toString();
  const pixelHeight = (canvas.clientHeight * dpr).toString();

  if (
    canvas.getAttribute("width") !== pixelWidth ||
    canvas.getAttribute("height") !== pixelHeight
  ) {
    canvas.setAttribute("width", pixelWidth);
    canvas.setAttribute("height", pixelHeight);
  }

  const context = canvas.getContext("webgpu");
  // null when the browser has no WebGPU support (e.g. Safari with the WebGPU
  // flag off).
  if (!context) {
    return null;
  }
  // On web there is no manual present; expose a no-op so RNCanvasContext's
  // present() (called after queue.submit() on native) is callable here too.
  return Object.assign(context, { present: () => {} });
}

// @ts-expect-error - polyfill for RNWebGPU native module
window.RNWebGPU = {
  getNativeSurface,
  // Unlike native, this can return null; Canvas.getContext() is typed
  // `RNCanvasContext | null` and passes it through.
  MakeWebGPUCanvasContext:
    makeWebGPUCanvasContext as typeof RNWebGPU.MakeWebGPUCanvasContext,
  // On web the browser owns the canvas/context lifecycle; nothing to retire.
  destroyContext: (_contextId: number) => {},
  fabric,
};
