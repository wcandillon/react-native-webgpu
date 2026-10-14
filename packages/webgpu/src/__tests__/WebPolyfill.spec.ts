import type { RNCanvasContext } from "../Canvas";

// The web polyfill for the RNWebGPU native module runs against the DOM, so it
// is exercised here with a minimal stand-in for `window` and `document`.

interface FakeCanvas {
  context: object | null;
  attributes: Record<string, string>;
  attributeWrites: number;
  calls: unknown[][];
  clientWidth: number;
  clientHeight: number;
  getAttribute(name: string): string | null;
  setAttribute(name: string, value: string): void;
  getContext(contextId: string): object | null;
  getBoundingClientRect(): { width: number; height: number };
  addEventListener(...args: unknown[]): void;
  removeEventListener(...args: unknown[]): void;
  dispatchEvent(...args: unknown[]): void;
  setPointerCapture(...args: unknown[]): void;
  releasePointerCapture(...args: unknown[]): void;
}

// Laid out at 100 x 50 CSS pixels and drawn at half that size under a CSS
// scale transform, which getBoundingClientRect() includes.
const makeCanvas = (context: object | null): FakeCanvas => ({
  context,
  attributes: {},
  attributeWrites: 0,
  calls: [],
  clientWidth: 100,
  clientHeight: 50,
  getAttribute(name) {
    return this.attributes[name] ?? null;
  },
  setAttribute(name, value) {
    this.attributes[name] = value;
    this.attributeWrites += 1;
  },
  getContext() {
    return this.context;
  },
  getBoundingClientRect: () => ({ width: 50, height: 25 }),
  addEventListener(...args) {
    this.calls.push(["addEventListener", ...args]);
  },
  removeEventListener(...args) {
    this.calls.push(["removeEventListener", ...args]);
  },
  dispatchEvent(...args) {
    this.calls.push(["dispatchEvent", ...args]);
  },
  setPointerCapture(...args) {
    this.calls.push(["setPointerCapture", ...args]);
  },
  releasePointerCapture(...args) {
    this.calls.push(["releasePointerCapture", ...args]);
  },
});

type Polyfill = Pick<typeof RNWebGPU, "getNativeSurface"> & {
  // Unlike native, the web polyfill returns null without WebGPU support.
  MakeWebGPUCanvasContext: (
    ...args: Parameters<typeof RNWebGPU.MakeWebGPUCanvasContext>
  ) => RNCanvasContext | null;
};

const loadPolyfill = (canvas: FakeCanvas): Polyfill => {
  const g = globalThis as Record<string, unknown>;
  g.window = { devicePixelRatio: 2 };
  g.document = { getElementById: () => canvas };
  jest.isolateModules(() => {
    require("../WebPolyfillGPUModule");
  });
  return (g.window as { RNWebGPU: Polyfill }).RNWebGPU;
};

describe("Web polyfill", () => {
  afterEach(() => {
    const g = globalThis as Record<string, unknown>;
    delete g.window;
    delete g.document;
  });

  it("returns the canvas context with a no-op present()", () => {
    const context = {};
    const RNWebGPU = loadPolyfill(makeCanvas(context));
    const result = RNWebGPU.MakeWebGPUCanvasContext(1, 0, 0);
    expect(result).toBe(context);
    expect(result!.present()).toBeUndefined();
  });

  it("sizes the drawing buffer from the canvas's layout size, not its transformed box", () => {
    const canvas = makeCanvas({});
    const RNWebGPU = loadPolyfill(canvas);
    const box = canvas.getBoundingClientRect();
    RNWebGPU.MakeWebGPUCanvasContext(1, box.width, box.height);
    expect(canvas.attributes).toEqual({ width: "200", height: "100" });
  });

  // Writing a canvas's width or height clears its drawing buffer.
  it("leaves a drawing buffer that already has its size untouched", () => {
    const canvas = makeCanvas({});
    const RNWebGPU = loadPolyfill(canvas);
    RNWebGPU.MakeWebGPUCanvasContext(1, 0, 0);
    const writes = canvas.attributeWrites;
    RNWebGPU.MakeWebGPUCanvasContext(1, 0, 0);
    expect({ writes, rewrites: canvas.attributeWrites - writes }).toEqual({
      writes: 2,
      rewrites: 0,
    });
  });

  it("reports the canvas's layout size as its native surface size", () => {
    const RNWebGPU = loadPolyfill(makeCanvas({}));
    const { width, height, clientWidth, clientHeight } =
      RNWebGPU.getNativeSurface(1);
    expect({ width, height, clientWidth, clientHeight }).toEqual({
      width: 100,
      height: 50,
      clientWidth: 100,
      clientHeight: 50,
    });
  });

  it("reads the native surface without touching the drawing buffer", () => {
    const canvas = makeCanvas({});
    loadPolyfill(canvas).getNativeSurface(1);
    expect(canvas.attributeWrites).toBe(0);
  });

  it("delegates the native surface's DOM methods to the canvas element", () => {
    const canvas = makeCanvas({});
    const surface = loadPolyfill(canvas).getNativeSurface(1);
    const listener = () => {};
    const event = new Event("pointerdown");
    surface.addEventListener("pointerdown", listener);
    surface.removeEventListener("pointerdown", listener);
    surface.dispatchEvent(event);
    surface.setPointerCapture(7);
    surface.releasePointerCapture(7);
    expect(canvas.calls).toEqual([
      ["addEventListener", "pointerdown", listener],
      ["removeEventListener", "pointerdown", listener],
      ["dispatchEvent", event],
      ["setPointerCapture", 7],
      ["releasePointerCapture", 7],
    ]);
  });

  it("returns null when the browser has no WebGPU context", () => {
    // canvas.getContext("webgpu") returns null when WebGPU is unavailable
    // (e.g. Safari with the WebGPU feature flag turned off).
    const RNWebGPU = loadPolyfill(makeCanvas(null));
    expect(RNWebGPU.MakeWebGPUCanvasContext(1, 0, 0)).toBeNull();
  });
});
