import React, { useState } from "react";
import { render, screen } from "@testing-library/react-native";

import { Canvas } from "../../mock";
import type { CanvasRef } from "../../mock";

const renderCanvas = async () => {
  const handles: (CanvasRef | null)[] = [];
  const ref = (handle: CanvasRef | null) => {
    handles.push(handle);
  };
  await render(<Canvas ref={ref} />);
  const [handle] = handles;
  if (!handle) {
    throw new Error("The mock Canvas did not attach its ref");
  }
  return { handle, handles, ref };
};

describe("The Jest mock's Canvas", () => {
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

  it("has no context, and fails loudly where a GPU is needed", async () => {
    const { handle } = await renderCanvas();
    expect(handle.getContextId()).toBe(-1);
    expect(handle.getContext("webgpu")).toBeNull();
    expect(() => handle.getNativeSurface()).toThrow(
      "Canvas.getNativeSurface() is not available under Jest",
    );
  });

  it("keeps its own props off the View it renders", async () => {
    await render(<Canvas testID="canvas" android={{ opaque: true }} />);
    const view = screen.getByTestId("canvas");
    expect(view.props).not.toHaveProperty("android");
  });
});
