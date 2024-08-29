import type { DrawingContext } from "./DrawingContext";

export class NativeDrawingContext implements DrawingContext {
  private texture: GPUTexture;
  private buffer: GPUBuffer;
  constructor(
    public device: GPUDevice,
    public width: number,
    public height: number,
  ) {
    const bytesPerRow = this.width * 4;
    this.texture = device.createTexture({
      size: [width, height],
      format: navigator.gpu.getPreferredCanvasFormat(),
      usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
    });
    this.buffer = device.createBuffer({
      size: bytesPerRow * this.height,
      usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
    });
  }

  getCurrentTexture() {
    return this.texture;
  }
  getImageData() {
    const commandEncoder = this.device.createCommandEncoder();
    const bytesPerRow = this.width * 4;
    commandEncoder.copyTextureToBuffer(
      { texture: this.texture },
      { buffer: this.buffer, bytesPerRow },
      [this.width, this.height],
    );
    this.device.queue.submit([commandEncoder.finish()]);

    return this.buffer.mapAsync(GPUMapMode.READ).then(() => {
      const arrayBuffer = this.buffer.getMappedRange();
      const uint8Array = new Uint8Array(arrayBuffer);
      const data = Array.from(uint8Array);
      this.buffer.unmap();
      return {
        data,
        width: this.width,
        height: this.height,
        format: navigator.gpu.getPreferredCanvasFormat(),
      };
    });
  }
}
