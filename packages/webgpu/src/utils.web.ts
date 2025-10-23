// Only used on the web
export function contextIdToId(contextId: number) {
  return "rnwgpu-canvas-" + contextId;
}

export const fabric = true;

export function getNativeSurface(contextId: number) {
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

export function makeWebGPUCanvasContext(
  contextId: number,
  width: number,
  height: number,
) {
  const canvas = document.getElementById(
    contextIdToId(contextId),
  ) as HTMLCanvasElement;

  if (
    canvas.getAttribute("width") !== width.toString() ||
    canvas.getAttribute("height") !== height.toString()
  ) {
    canvas.setAttribute("width", width.toString());
    canvas.setAttribute("height", height.toString());
  }

  const context = canvas.getContext("webgpu")!;

  return Object.assign(context, {
    present: () => {},
  });
}
