import { client, isNativeApp } from "./setup";

// The harness mounts a canvas laid out at a known size and drawn at half that
// size under a scale transform (apps/example/src/Tests.tsx).

describe("Canvas", () => {
  it("sizes the context's canvas from its layout, not its transformed box", async () => {
    if (!isNativeApp()) {
      return;
    }
    const result = await client.eval(({ views }) => {
      const context = views?.scaledCanvas()?.getContext("webgpu");
      if (!views || !context) {
        throw new Error("The Tests screen has no scaled canvas");
      }
      const canvas = context.canvas as HTMLCanvasElement;
      return {
        layoutSize: views.scaledCanvasSize,
        size: { width: canvas.width, height: canvas.height },
        clientSize: { width: canvas.clientWidth, height: canvas.clientHeight },
      };
    });
    expect(result.size).toEqual(result.layoutSize);
    expect(result.clientSize).toEqual(result.layoutSize);
  });

  it("keeps one ref handle across re-renders", async () => {
    if (!isNativeApp()) {
      return;
    }
    const result = await client.eval(({ views }) => {
      if (!views) {
        throw new Error("The Tests screen has no scaled canvas");
      }
      const { scaledCanvas, rerender } = views;
      const handle = scaledCanvas();
      return Promise.all([rerender(), rerender()])
        .then(() => rerender())
        .then(() => ({
          attached: handle !== null,
          kept: scaledCanvas() === handle,
        }));
    });
    expect(result).toEqual({ attached: true, kept: true });
  });
});
