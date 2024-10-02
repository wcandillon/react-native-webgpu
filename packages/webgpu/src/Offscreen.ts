/* eslint-disable @typescript-eslint/no-explicit-any */
export class GPUOffscreenCanvas implements OffscreenCanvas {
  width: number;
  height: number;
  oncontextlost: ((this: OffscreenCanvas, ev: Event) => any) | null = null;
  oncontextrestored: ((this: OffscreenCanvas, ev: Event) => any) | null = null;

  private context: GPUOffscreenCanvasContext;

  constructor(width: number, height: number) {
    this.width = width;
    this.height = height;
    this.context = new GPUOffscreenCanvasContext(this);
  }

  convertToBlob(_options?: ImageEncodeOptions): Promise<Blob> {
    // Implementation for converting the canvas content to a Blob
    throw new Error("Method not implemented.");
  }

  // Overloaded method signatures
  getContext(
    contextId: "2d",
    options?: any,
  ): OffscreenCanvasRenderingContext2D | null;
  getContext(
    contextId: "bitmaprenderer",
    options?: any,
  ): ImageBitmapRenderingContext | null;
  getContext(contextId: "webgl", options?: any): WebGLRenderingContext | null;
  getContext(contextId: "webgl2", options?: any): WebGL2RenderingContext | null;
  getContext(
    contextId: OffscreenRenderingContextId,
    options?: any,
  ): OffscreenRenderingContext | null;
  getContext(contextId: "webgpu"): GPUCanvasContext | null;
  getContext(
    contextId: unknown,
    _options?: any,
  ): OffscreenRenderingContext | GPUCanvasContext | null {
    if (contextId === "webgpu") {
      return this.context;
    }
    // Implement other context types if necessary
    return null;
  }

  transferToImageBitmap(): ImageBitmap {
    // Implementation for transferring the canvas content to an ImageBitmap
    throw new Error("Method not implemented.");
  }

  addEventListener<K extends keyof OffscreenCanvasEventMap>(
    _type: K,
    _listener: (this: OffscreenCanvas, ev: OffscreenCanvasEventMap[K]) => any,
    _options?: boolean | AddEventListenerOptions,
  ): void {
    // Event listener implementation
    throw new Error("Method not implemented.");
  }

  removeEventListener<K extends keyof OffscreenCanvasEventMap>(
    _type: K,
    _listener: (this: OffscreenCanvas, ev: OffscreenCanvasEventMap[K]) => any,
    _options?: boolean | EventListenerOptions,
  ): void {
    // Remove event listener implementation
    throw new Error("Method not implemented.");
  }

  dispatchEvent(_event: Event): boolean {
    // Event dispatch implementation
    throw new Error("Method not implemented.");
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

  constructor(public readonly canvas: OffscreenCanvas) {
    this.textureFormat = navigator.gpu.getPreferredCanvasFormat();
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
