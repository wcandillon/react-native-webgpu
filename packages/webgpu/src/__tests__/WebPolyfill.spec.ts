// The web polyfill for the RNWebGPU native module runs against the DOM, so it
// is exercised here with a minimal stand-in for `window` and `document`.

interface FakeCanvas {
  context: object | null;
  attributes: Record<string, string>;
  clientWidth: number;
  clientHeight: number;
  getAttribute(name: string): string | null;
  setAttribute(name: string, value: string): void;
  getContext(contextId: string): object | null;
  getBoundingClientRect(): { width: number; height: number };
  addEventListener(): void;
  removeEventListener(): void;
  dispatchEvent(): void;
  setPointerCapture(): void;
  releasePointerCapture(): void;
}

// Laid out at 100 x 50 CSS pixels and drawn at half that size under a CSS
// scale transform, which getBoundingClientRect() includes.
const makeCanvas = (context: object | null): FakeCanvas => ({
  context,
  attributes: {},
  clientWidth: 100,
  clientHeight: 50,
  getAttribute(name) {
    return this.attributes[name] ?? null;
  },
  setAttribute(name, value) {
    this.attributes[name] = value;
  },
  getContext() {
    return this.context;
  },
  getBoundingClientRect: () => ({ width: 50, height: 25 }),
  addEventListener() {},
  removeEventListener() {},
  dispatchEvent() {},
  setPointerCapture() {},
  releasePointerCapture() {},
});

type Polyfill = {
  MakeWebGPUCanvasContext: (
    contextId: number,
    width: number,
    height: number,
  ) => (object & { present: () => void }) | null;
  getNativeSurface: (contextId: number) => {
    width: number;
    height: number;
    clientWidth: number;
    clientHeight: number;
  };
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
    const canvas = makeCanvas(context);
    const RNWebGPU = loadPolyfill(canvas);
    const result = RNWebGPU.MakeWebGPUCanvasContext(1, 100, 50);
    expect(result).toBe(context);
    expect(typeof result!.present).toBe("function");
    expect(canvas.attributes).toEqual({ width: "200", height: "100" });
  });

  it("sizes the drawing buffer from the canvas's layout size, not its transformed box", () => {
    const canvas = makeCanvas({});
    const RNWebGPU = loadPolyfill(canvas);
    const box = canvas.getBoundingClientRect();
    RNWebGPU.MakeWebGPUCanvasContext(1, box.width, box.height);
    expect(canvas.attributes).toEqual({ width: "200", height: "100" });
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

  it("returns null when the browser has no WebGPU context", () => {
    // canvas.getContext("webgpu") returns null when WebGPU is unavailable
    // (e.g. Safari with the WebGPU feature flag turned off).
    const RNWebGPU = loadPolyfill(makeCanvas(null));
    expect(RNWebGPU.MakeWebGPUCanvasContext(1, 100, 50)).toBeNull();
  });
});
