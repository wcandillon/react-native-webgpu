export * from "../Canvas";
export * from "../Offscreen";
export * from "../WebGPUViewNativeComponent";
export * from "../hooks";
export * from "../types";

// We don't need to set all global properties on web, webgpu is already available globally

RNWebGPU.fabric = true;
RNWebGPU.getNativeSurface = (contextId: number) => {
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
};

function contextIdToId(contextId: number) {
  return "rnwgpu-canvas-" + contextId;
}

RNWebGPU.MakeWebGPUCanvasContext = (
  contextId: number,
  width: number,
  height: number,
) => {
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
};
