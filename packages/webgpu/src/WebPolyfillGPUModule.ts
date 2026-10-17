import type { NativeCanvas } from "./types";
import { contextIdToId } from "./utils";

const fabric = true;

function getCanvasElement(contextId: number) {
  return document.getElementById(
    contextIdToId(contextId),
  ) as HTMLCanvasElement | null;
}

// Native's answer for a context it no longer knows (RNWebGPU::getNativeSurface).
const makeRetiredSurface = (): NativeCanvas => ({
  surface: BigInt(0),
  height: 0,
  width: 0,
  clientHeight: 0,
  clientWidth: 0,
  addEventListener: () => {},
  removeEventListener: () => {},
  dispatchEvent: () => {},
  setPointerCapture: () => {},
  releasePointerCapture: () => {},
});

function getNativeSurface(contextId: number): NativeCanvas {
  const canvas = getCanvasElement(contextId);
  if (canvas === null) {
    return makeRetiredSurface();
  }

  // Layout size, like the native view size in dp: unlike
  // getBoundingClientRect() it ignores CSS transforms.
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

function makeWebGPUCanvasContext(
  contextId: number,
  width: number,
  height: number,
) {
  const canvas = getCanvasElement(contextId);
  if (canvas === null) {
    return null;
  }

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

const polyfill: Pick<
  typeof RNWebGPU,
  "getNativeSurface" | "MakeWebGPUCanvasContext" | "destroyContext" | "fabric"
> = {
  getNativeSurface,
  MakeWebGPUCanvasContext: makeWebGPUCanvasContext,
  // On web the browser owns the canvas/context lifecycle; nothing to retire.
  destroyContext: (_contextId: number) => {},
  fabric,
};

// The web provides these members of the native module and no others.
window.RNWebGPU = polyfill as typeof RNWebGPU;
