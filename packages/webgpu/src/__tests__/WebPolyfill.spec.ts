// The web polyfill for the RNWebGPU native module runs against the DOM, so it
// is exercised here with a minimal stand-in for `window` and `document`.

interface FakeCanvas {
  context: object | null;
  attributes: Record<string, string>;
  getAttribute(name: string): string | null;
  setAttribute(name: string, value: string): void;
  getContext(contextId: string): object | null;
}

const makeCanvas = (context: object | null): FakeCanvas => ({
  context,
  attributes: {},
  getAttribute(name) {
    return this.attributes[name] ?? null;
  },
  setAttribute(name, value) {
    this.attributes[name] = value;
  },
  getContext() {
    return this.context;
  },
});

type Polyfill = Pick<
  typeof RNWebGPU,
  "getNativeSurface" | "MakeWebGPUCanvasContext"
>;

const loadPolyfill = (canvas: FakeCanvas | null): Polyfill => {
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

  it("returns null when the browser has no WebGPU context", () => {
    // canvas.getContext("webgpu") returns null when WebGPU is unavailable
    // (e.g. Safari with the WebGPU feature flag turned off).
    const RNWebGPU = loadPolyfill(makeCanvas(null));
    expect(RNWebGPU.MakeWebGPUCanvasContext(1, 100, 50)).toBeNull();
  });

  it("reports a 0 x 0 surface once the canvas element is gone, as native does", () => {
    const surface = loadPolyfill(null).getNativeSurface(1);
    expect({
      surface: surface.surface,
      width: surface.width,
      height: surface.height,
      clientWidth: surface.clientWidth,
      clientHeight: surface.clientHeight,
    }).toEqual({
      surface: BigInt(0),
      width: 0,
      height: 0,
      clientWidth: 0,
      clientHeight: 0,
    });
    expect(() => {
      surface.addEventListener("pointerdown", () => {});
      surface.removeEventListener("pointerdown", () => {});
      surface.dispatchEvent(new Event("pointerdown"));
      surface.setPointerCapture(7);
      surface.releasePointerCapture(7);
    }).not.toThrow();
  });

  it("returns no context once the canvas element is gone", () => {
    expect(loadPolyfill(null).MakeWebGPUCanvasContext(1, 100, 50)).toBeNull();
  });
});
