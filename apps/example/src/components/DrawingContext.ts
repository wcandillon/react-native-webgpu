export interface DrawingContext {
  width: number;
  height: number;
  getCurrentTexture(): GPUTexture;
  getImageData(): Promise<{
    data: number[];
    width: number;
    height: number;
    format: string;
  }>;
}
