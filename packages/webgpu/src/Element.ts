import WebGPUModule from "./NativeWebGPUModule";
import type {
  CapturedElement,
  GPUCopyElementImageDestination,
  GPUCopyElementImageSource,
} from "./types";

// "HTML in Canvas" for React Native: paint a native child view of a
// <Canvas layoutSubtree> into a GPUTexture, mirroring the WICG
// GPUQueue.copyElementImageToTexture proposal.
//
// Unlike the web API (which is synchronous because the engine already holds the
// element's painted layer), this is async: the native view is rendered on
// demand into an AHardwareBuffer (Android) / IOSurface (iOS, future), whose
// GPU-completion fence is delivered asynchronously. The captured surface is
// then imported via the existing SharedTextureMemory + SharedFence path and
// copied into the destination texture.

// Maps a GPUQueue back to the GPUDevice that owns it, so the spec-shaped
// `device.queue.copyElementImageToTexture(...)` can reach the device-level
// importSharedTextureMemory / importSharedFence. Populated by wrapping the
// device's `queue` getter in installElementCapture().
const deviceForQueue = new WeakMap<GPUQueue, GPUDevice>();

const normalizeOrigin = (origin?: GPUOrigin3D): [number, number] => {
  if (!origin) {
    return [0, 0];
  }
  if (Array.isArray(origin)) {
    return [origin[0] ?? 0, origin[1] ?? 0];
  }
  const dict = origin as GPUOrigin3DDict;
  return [dict.x ?? 0, dict.y ?? 0];
};

export const copyElementImageToTexture = async (
  device: GPUDevice,
  source: GPUCopyElementImageSource,
  destination: GPUCopyElementImageDestination,
): Promise<void> => {
  // Render the view off-screen and hand the native surface to C++. `token` is a
  // plain int over the bridge; the 64-bit pointers come back via JSI BigInts.
  const token = await WebGPUModule.captureElement(source.source);
  const captured = RNWebGPU.consumeCapturedElement(token) as CapturedElement;

  const memory = device.importSharedTextureMemory({ handle: captured.handle });
  // The texture aliases the captured surface; we read it as a copy source.
  const srcTexture = memory.createTexture();

  const fences =
    captured.fence !== 0n
      ? [
          {
            fence: device.importSharedFence({
              type: captured.fenceType,
              handle: captured.fence,
            }),
            // sync-fd / mtl-shared-event producer fences are binary (value 0).
            signaledValue: 0n,
          },
        ]
      : [];

  // The surface already holds the painted view, so it is initialized; Dawn
  // waits on the producer fence before the copy below reads it.
  memory.beginAccess(srcTexture, true, fences);
  try {
    // The producer-reported size (captured.width/height) can differ from the
    // imported texture's real dimensions, so srcTexture.width/height (Dawn's
    // truth) is authoritative. Clamp the copy to what both textures can provide
    // so an over-large request never trips copy-range validation.
    const sx = Math.max(0, Math.floor(source.sx ?? 0));
    const sy = Math.max(0, Math.floor(source.sy ?? 0));
    const [dox, doy] = normalizeOrigin(destination.origin);
    const srcAvailW = Math.max(0, srcTexture.width - sx);
    const srcAvailH = Math.max(0, srcTexture.height - sy);
    const dstAvailW = Math.max(0, destination.texture.width - dox);
    const dstAvailH = Math.max(0, destination.texture.height - doy);
    const width = Math.min(
      destination.width ?? source.swidth ?? srcAvailW,
      srcAvailW,
      dstAvailW,
    );
    const height = Math.min(
      destination.height ?? source.sheight ?? srcAvailH,
      srcAvailH,
      dstAvailH,
    );
    if (width > 0 && height > 0) {
      const encoder = device.createCommandEncoder();
      encoder.copyTextureToTexture(
        { texture: srcTexture, origin: { x: sx, y: sy } },
        {
          texture: destination.texture,
          mipLevel: destination.mipLevel ?? 0,
          origin: destination.origin,
        },
        { width, height, depthOrArrayLayers: 1 },
      );
      device.queue.submit([encoder.finish()]);
    }
  } finally {
    // endAccess is the post-submit release: Dawn keeps the surface alive for the
    // in-flight copy via its own fences.
    memory.endAccess(srcTexture);
    // Drop the reference the producer handed us; the import above took its own,
    // so the native buffer lives as long as `memory` does.
    RNWebGPU.releaseCapturedElement(captured.handle);
  }
};

// Installs the spec-shaped `device.queue.copyElementImageToTexture` method and
// the device->queue back-reference it relies on. Safe to call more than once.
export const installElementCapture = () => {
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const g = globalThis as any;
  const GPUDeviceCtor = g.GPUDevice;
  const GPUQueueCtor = g.GPUQueue;
  if (!GPUDeviceCtor || !GPUQueueCtor) {
    return;
  }

  const desc = Object.getOwnPropertyDescriptor(
    GPUDeviceCtor.prototype,
    "queue",
  );
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  if (desc?.get && !(desc.get as any).__rnwgpuWrapped) {
    const orig = desc.get;
    function wrappedQueueGetter(this: GPUDevice) {
      const queue = orig.call(this) as GPUQueue;
      deviceForQueue.set(queue, this);
      return queue;
    }
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    (wrappedQueueGetter as any).__rnwgpuWrapped = true;
    Object.defineProperty(GPUDeviceCtor.prototype, "queue", {
      ...desc,
      get: wrappedQueueGetter,
    });
  }

  if (!GPUQueueCtor.prototype.copyElementImageToTexture) {
    GPUQueueCtor.prototype.copyElementImageToTexture = function (
      this: GPUQueue,
      source: GPUCopyElementImageSource,
      destination: GPUCopyElementImageDestination,
    ) {
      const device = deviceForQueue.get(this);
      if (!device) {
        throw new Error(
          "[WebGPU] queue.copyElementImageToTexture: owning device not found. " +
            "Access the queue as `device.queue` before calling, or use the " +
            "exported copyElementImageToTexture(device, source, destination).",
        );
      }
      return copyElementImageToTexture(device, source, destination);
    };
  }
};
