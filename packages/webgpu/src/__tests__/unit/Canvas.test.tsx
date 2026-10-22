import React, { useState } from "react";
import { View } from "react-native";
import { render, screen } from "@testing-library/react-native";

import { Canvas } from "../../Canvas";
import type { CanvasRef, RNCanvasContext } from "../../Canvas";

const LAYOUT_SIZE = { width: 64, height: 32 };

const context = {} as RNCanvasContext;
const nativeModule: Pick<
  typeof RNWebGPU,
  "MakeWebGPUCanvasContext" | "getNativeSurface" | "destroyContext"
> = {
  MakeWebGPUCanvasContext: jest.fn(() => context),
  getNativeSurface: jest.fn(),
  destroyContext: jest.fn(),
};
Object.assign(globalThis, { RNWebGPU: nativeModule });

const measureLayout = jest.spyOn(View.prototype, "measureLayout");

beforeEach(() => {
  jest.clearAllMocks();
  measureLayout.mockImplementation((_relativeTo, onSuccess) =>
    onSuccess(0, 0, LAYOUT_SIZE.width, LAYOUT_SIZE.height),
  );
});

const renderCanvas = async () => {
  const handles: (CanvasRef | null)[] = [];
  const ref = (handle: CanvasRef | null) => {
    handles.push(handle);
  };
  await render(<Canvas ref={ref} />);
  const [handle] = handles;
  if (!handle) {
    throw new Error("Canvas did not attach its ref");
  }
  return { handle, handles, ref };
};

describe("Canvas", () => {
  it("creates the context at the layout size the view measures relative to itself", async () => {
    const { handle } = await renderCanvas();
    expect(handle.getContext("webgpu")).toBe(context);
    expect(nativeModule.MakeWebGPUCanvasContext).toHaveBeenCalledWith(
      handle.getContextId(),
      LAYOUT_SIZE.width,
      LAYOUT_SIZE.height,
    );
    expect(measureLayout.mock.calls[0][0]).toBe(measureLayout.mock.contexts[0]);
  });

  it("keeps one ref handle across re-renders", async () => {
    const { handles, ref } = await renderCanvas();
    await screen.rerender(<Canvas ref={ref} />);
    await screen.rerender(<Canvas ref={ref} android={{ opaque: true }} />);
    expect(handles).toHaveLength(1);
  });

  it("settles when a state setter is its ref", async () => {
    let renders = 0;
    const Owner = () => {
      const [, setCanvas] = useState<CanvasRef | null>(null);
      renders += 1;
      return <Canvas ref={setCanvas} />;
    };
    await render(<Owner />);
    expect(renders).toBe(2);
  });

  it("retires its context once, on unmount", async () => {
    const { handle } = await renderCanvas();
    expect(nativeModule.destroyContext).not.toHaveBeenCalled();
    await screen.unmount();
    expect(nativeModule.destroyContext).toHaveBeenCalledTimes(1);
    expect(nativeModule.destroyContext).toHaveBeenCalledWith(
      handle.getContextId(),
    );
  });

  it("rejects a context name other than webgpu", async () => {
    const { handle } = await renderCanvas();
    const getContext = handle.getContext as (contextName: string) => unknown;
    expect(() => getContext("webgl")).toThrow(
      "[WebGPU] Unsupported context: webgl",
    );
  });
});
