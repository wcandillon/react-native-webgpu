import type { SkSurface } from "@shopify/react-native-skia";
import {
  AlphaType,
  BlendMode,
  ColorType,
  Skia,
} from "@shopify/react-native-skia";
import { data } from "@tensorflow/tfjs";
import { Color } from "three";

export class SkiaContext implements OffscreenCanvasRenderingContext2D {
  private surface: SkSurface;

  constructor(width: number, height: number) {
    this.surface = Skia.Surface.MakeOffscreen(width, height)!;
  }

  get canvas() {
    return this.surface.getCanvas();
  }

  get width() {
    return this.surface.width();
  }

  get height() {
    return this.surface.height();
  }

  canvas: OffscreenCanvas;
  globalAlpha: number;
  globalCompositeOperation: GlobalCompositeOperation;

  drawImage(
    image: unknown,
    sx: number,
    sy: number,
    sw?: number,
    sh?: number,
    dx?: number,
    dy?: number,
    dw?: number,
    dh?: number,
  ): void {
    const ctx = (image as any).getContext("webgpu");
    const texture: GPUTexture = ctx.getTexture();
    const device: GPUDevice = ctx.getDevice();
    const textureWidth = sw || texture.width;
    const textureHeight = sh || texture.height;

    const bufferSize = textureWidth * textureHeight * 4; // Assuming 4 bytes per pixel (RGBA8 format)
    const buffer = device.createBuffer({
      size: bufferSize,
      usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
    });

    const commandEncoder = device.createCommandEncoder();
    commandEncoder.copyTextureToBuffer(
      { texture: texture, origin: { x: sx, y: sy } },
      { buffer: buffer, bytesPerRow: textureWidth * 4 },
      { width: textureWidth, height: textureHeight, depthOrArrayLayers: 1 },
    );

    const commands = commandEncoder.finish();
    device.queue.submit([commands]);
    console.log("mapAsync");
    buffer.mapAsync(GPUMapMode.READ).then(() => {
      console.log("mapAsync done");
      const arrayBuffer = buffer.getMappedRange();
      const typedArray = new Uint8Array(arrayBuffer);
      console.log(typedArray.slice(0, 20));
      const d = Skia.Data.fromBytes(typedArray);
      const img = Skia.Image.MakeImage(
        {
          alphaType: AlphaType.Premul,
          colorType: ColorType.RGBA_8888,
          height: textureWidth,
          width: textureHeight,
        },
        d,
        textureWidth * 4,
      );
      this.canvas.drawImage(img!, dx || 0, dy || 0);
      buffer.unmap();
    });
  }
  beginPath(): void {
    throw new Error("Method not implemented.");
  }
  clip(_path?: unknown, _fillRule?: unknown): void {
    throw new Error("Method not implemented.");
  }
  fill(_path?: unknown, _fillRule?: unknown): void {
    throw new Error("Method not implemented.");
  }
  isPointInPath(
    _path: unknown,
    _x: unknown,
    _y?: unknown,
    _fillRule?: unknown,
  ): boolean {
    throw new Error("Method not implemented.");
  }
  isPointInStroke(_path: unknown, _x: unknown, _y?: unknown): boolean {
    throw new Error("Method not implemented.");
  }
  stroke(_path?: unknown): void {
    throw new Error("Method not implemented.");
  }
  fillStyle: string | CanvasGradient | CanvasPattern;
  strokeStyle: string | CanvasGradient | CanvasPattern;
  createConicGradient(
    _startAngle: number,
    _x: number,
    _y: number,
  ): CanvasGradient {
    throw new Error("Method not implemented.");
  }
  createLinearGradient(
    _x0: number,
    _y0: number,
    _x1: number,
    _y1: number,
  ): CanvasGradient {
    throw new Error("Method not implemented.");
  }
  createPattern(
    _image: CanvasImageSource,
    _repetition: string | null,
  ): CanvasPattern | null {
    throw new Error("Method not implemented.");
  }
  createRadialGradient(
    _x0: number,
    _y0: number,
    _r0: number,
    _x1: number,
    _y1: number,
    _r1: number,
  ): CanvasGradient {
    throw new Error("Method not implemented.");
  }
  filter: string;
  createImageData(_sw: unknown, _sh?: unknown, _settings?: unknown): ImageData {
    throw new Error("Method not implemented.");
  }
  getImageData(
    _sx: number,
    _sy: number,
    _sw: number,
    _sh: number,
    _settings?: ImageDataSettings,
  ): ImageData {
    console.log("getImageData");
    this.surface.flush();
    const imageData = {
      data: this.canvas.readPixels(0, 0, {
        width: this.width,
        height: this.height,
        colorType: ColorType.RGBA_8888,
        alphaType: AlphaType.Premul,
      }),
      width: this.width,
      height: this.height,
      colorSpace: "srgb",
    };
    return imageData;
  }
  putImageData(
    _imagedata: unknown,
    _dx: unknown,
    _dy: unknown,
    _dirtyX?: unknown,
    _dirtyY?: unknown,
    _dirtyWidth?: unknown,
    _dirtyHeight?: unknown,
  ): void {
    throw new Error("Method not implemented.");
  }
  imageSmoothingEnabled: boolean;
  imageSmoothingQuality: ImageSmoothingQuality;
  arc(
    _x: number,
    _y: number,
    _radius: number,
    _startAngle: number,
    _endAngle: number,
    _counterclockwise?: boolean,
  ): void {
    throw new Error("Method not implemented.");
  }
  arcTo(
    _x1: number,
    _y1: number,
    _x2: number,
    _y2: number,
    _radius: number,
  ): void {
    throw new Error("Method not implemented.");
  }
  bezierCurveTo(
    _cp1x: number,
    _cp1y: number,
    _cp2x: number,
    _cp2y: number,
    _x: number,
    _y: number,
  ): void {
    throw new Error("Method not implemented.");
  }
  closePath(): void {
    throw new Error("Method not implemented.");
  }
  ellipse(
    _x: number,
    _y: number,
    _radiusX: number,
    _radiusY: number,
    _rotation: number,
    _startAngle: number,
    _endAngle: number,
    _counterclockwise?: boolean,
  ): void {
    throw new Error("Method not implemented.");
  }
  lineTo(_x: number, _y: number): void {
    throw new Error("Method not implemented.");
  }
  moveTo(_x: number, _y: number): void {
    throw new Error("Method not implemented.");
  }
  quadraticCurveTo(_cpx: number, _cpy: number, _x: number, _y: number): void {
    throw new Error("Method not implemented.");
  }
  rect(_x: number, _y: number, _w: number, _h: number): void {
    throw new Error("Method not implemented.");
  }
  roundRect(
    _x: number,
    _y: number,
    _w: number,
    _h: number,
    _radii?: number | DOMPointInit | (number | DOMPointInit)[],
  ): void {
    throw new Error("Method not implemented.");
  }
  lineCap: CanvasLineCap;
  lineDashOffset: number;
  lineJoin: CanvasLineJoin;
  lineWidth: number;
  miterLimit: number;
  getLineDash(): number[] {
    throw new Error("Method not implemented.");
  }
  setLineDash(_segments: number[]): void {
    throw new Error("Method not implemented.");
  }
  clearRect(x: number, y: number, w: number, h: number): void {
    const rect = Skia.XYWHRect(x, y, w, h);
    const paint = Skia.Paint();
    paint.setBlendMode(BlendMode.Clear);
    paint.setColor(Skia.Color("transparent"));
    this.canvas.drawRect(rect, paint);
  }
  fillRect(_x: number, _y: number, _w: number, _h: number): void {
    throw new Error("Method not implemented.");
  }
  strokeRect(_x: number, _y: number, _w: number, _h: number): void {
    throw new Error("Method not implemented.");
  }
  shadowBlur: number;
  shadowColor: string;
  shadowOffsetX: number;
  shadowOffsetY: number;
  isContextLost(): boolean {
    throw new Error("Method not implemented.");
  }
  reset(): void {
    throw new Error("Method not implemented.");
  }
  restore(): void {
    throw new Error("Method not implemented.");
  }
  save(): void {
    throw new Error("Method not implemented.");
  }
  fillText(_text: string, _x: number, _y: number, _maxWidth?: number): void {
    throw new Error("Method not implemented.");
  }
  measureText(_text: string): TextMetrics {
    throw new Error("Method not implemented.");
  }
  strokeText(_text: string, _x: number, _y: number, _maxWidth?: number): void {
    throw new Error("Method not implemented.");
  }
  direction: CanvasDirection;
  font: string;
  fontKerning: CanvasFontKerning;
  fontStretch: CanvasFontStretch;
  fontVariantCaps: CanvasFontVariantCaps;
  letterSpacing: string;
  textAlign: CanvasTextAlign;
  textBaseline: CanvasTextBaseline;
  textRendering: CanvasTextRendering;
  wordSpacing: string;
  getTransform(): DOMMatrix {
    throw new Error("Method not implemented.");
  }
  resetTransform(): void {
    throw new Error("Method not implemented.");
  }
  rotate(_angle: number): void {
    throw new Error("Method not implemented.");
  }
  scale(_x: number, _y: number): void {
    throw new Error("Method not implemented.");
  }
  setTransform(
    _a?: unknown,
    _b?: unknown,
    _c?: unknown,
    _d?: unknown,
    _e?: unknown,
    _f?: unknown,
  ): void {
    throw new Error("Method not implemented.");
  }
  transform(
    _a: number,
    _b: number,
    _c: number,
    _d: number,
    _e: number,
    _f: number,
  ): void {
    throw new Error("Method not implemented.");
  }
  translate(_x: number, _y: number): void {
    throw new Error("Method not implemented.");
  }
}
