export class DrawingContext {
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
      format: gpu.getPreferredCanvasFormat(),
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

  present(commandEncoder: GPUCommandEncoder) {
    const bytesPerRow = this.width * 4;
    commandEncoder.copyTextureToBuffer(
      { texture: this.texture },
      { buffer: this.buffer, bytesPerRow },
      [this.width, this.height],
    );
  }

  getImageData() {
    return this.buffer.mapAsync(GPUMapMode.READ).then(() => {
      const arrayBuffer = this.buffer.getMappedRange();
      const uint8Array = new Uint8Array(arrayBuffer);
      const r = Array.from(uint8Array);
      this.buffer.unmap();
      return r;
    });
  }
}
