export class GPUOffscreenCanvas {
  width: number;
  height: number;

  private context: GPUOffscreenCanvasContext;

  constructor(width: number, height: number) {
    this.width = width;
    this.height = height;
    this.context = new GPUOffscreenCanvasContext(this);
  }

  getContext(contextId: "webgpu"): GPUOffscreenCanvasContext | null {
    if (contextId === "webgpu") {
      return this.context;
    }
    // Implement other context types if necessary
    return null;
  }

  getImageData() {
    const device = this.context.getDevice();
    const texture = this.context.getTexture();
    const commandEncoder = device.createCommandEncoder();
    const bytesPerRow = this.width * 4;
    const buffer = device.createBuffer({
      size: bytesPerRow * this.height,
      usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
    });
    commandEncoder.copyTextureToBuffer(
      { texture: texture },
      { buffer: buffer, bytesPerRow },
      [this.width, this.height],
    );
    device.queue.submit([commandEncoder.finish()]);

    return buffer.mapAsync(GPUMapMode.READ).then(() => {
      const arrayBuffer = buffer.getMappedRange();
      const uint8Array = new Uint8Array(arrayBuffer);
      const data = Array.from(uint8Array);
      buffer.unmap();
      return {
        data,
        width: this.width,
        height: this.height,
        format: navigator.gpu.getPreferredCanvasFormat(),
      };
    });
  }
}

class GPUOffscreenCanvasContext implements GPUCanvasContext {
  __brand = "GPUCanvasContext" as const;

  private textureFormat: GPUTextureFormat;
  private texture: GPUTexture | null = null;
  private device: GPUDevice | null = null;

  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  constructor(public readonly canvas: any) {
    this.textureFormat = navigator.gpu.getPreferredCanvasFormat();
  }
  getConfiguration(): GPUCanvasConfigurationOut | null {
    throw new Error("Method not implemented.");
  }

  present() {
    // Do nothing
  }

  getDevice() {
    if (!this.device) {
      throw new Error("Device is not configured.");
    }
    return this.device;
  }

  getTexture() {
    if (!this.texture) {
      throw new Error("Texture is not configured");
    }
    return this.texture;
  }

  configure(config: GPUCanvasConfiguration) {
    // Configure the canvas context with the device and format
    this.device = config.device;
    this.texture = config.device.createTexture({
      size: [this.canvas.width, this.canvas.height],
      format: this.textureFormat,
      usage:
        GPUTextureUsage.RENDER_ATTACHMENT |
        GPUTextureUsage.COPY_SRC |
        GPUTextureUsage.TEXTURE_BINDING,
    });
    return undefined;
  }

  unconfigure() {
    // Unconfigure the canvas context
    if (this.texture) {
      this.texture.destroy();
    }
    return undefined;
  }

  getCurrentTexture(): GPUTexture {
    if (!this.texture) {
      throw new Error("Texture is not configured");
    }
    return this.texture;
  }
}
