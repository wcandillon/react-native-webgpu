/* eslint-disable max-len */
/* eslint-disable @typescript-eslint/no-explicit-any */
/* eslint-disable no-var */
import fs from "fs";
import path from "path";

import type { Server, WebSocket } from "ws";
import type { Browser, Page } from "puppeteer";
import puppeteer from "puppeteer";
import { PNG } from "pngjs";
import pixelmatch from "pixelmatch";

import { cubeVertexArray } from "../../example/src/components/cube";
import {
  redFragWGSL,
  triangleVertWGSL,
} from "../../example/src/components/triangle";

import { DEBUG, REFERENCE } from "./config";

jest.setTimeout(180 * 1000);

type TestOS = "ios" | "android" | "web" | "node";

declare global {
  var testServer: Server;
  var testClient: WebSocket;
  var testOS: TestOS;
}
export let client: TestingClient;

beforeAll(async () => {
  client = REFERENCE ? new ReferenceTestingClient() : new RemoteTestingClient();
  await client.init();
});

afterAll(async () => {
  await client.dispose();
});

interface DrawingContext {
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

interface GPUContext {
  gpu: GPU;
  adapter: GPUAdapter;
  device: GPUDevice;
  GPUBufferUsage: typeof GPUBufferUsage;
  GPUColorWrite: typeof GPUColorWrite;
  GPUMapMode: typeof GPUMapMode;
  GPUShaderStage: typeof GPUShaderStage;
  GPUTextureUsage: typeof GPUTextureUsage;
  cubeVertexArray: Float32Array;
  triangleVertWGSL: string;
  redFragWGSL: string;
  ctx: DrawingContext;
}

type Ctx = Record<string, unknown>;

type JSONValue =
  | { [key: string]: JSONValue }
  | JSONValue[]
  | number
  | string
  | boolean
  | null;

interface TestingClient {
  eval<C = Ctx, R extends JSONValue = JSONValue>(
    fn: (ctx: GPUContext & C) => R | Promise<R>,
    ctx?: C,
  ): Promise<R>;
  OS: TestOS;
  arch: "paper" | "fabric";
  init(): Promise<void>;
  dispose(): Promise<void>;
}

class RemoteTestingClient implements TestingClient {
  readonly OS = global.testOS;
  readonly arch = global.testArch;

  eval<C = Ctx, R extends JSONValue = JSONValue>(
    fn: (ctx: GPUContext & C) => R | Promise<R>,
    context?: C,
  ): Promise<R> {
    const ctx = this.prepareContext(context ?? {});
    const body = { code: fn.toString(), ctx };
    return this.handleResponse<R>(JSON.stringify(body));
  }

  private handleResponse<R>(body: string): Promise<R> {
    return new Promise((resolve) => {
      this.client.once("message", (raw: Buffer) => {
        resolve(JSON.parse(raw.toString()));
      });
      this.client.send(body);
    });
  }

  private get client() {
    if (global.testClient === null) {
      throw new Error("Client is not connected. Did you call init?");
    }
    return global.testClient!;
  }

  private prepareContext<C extends Ctx>(context?: C): C {
    const ctx: any = {};
    if (context) {
      for (const [key, value] of Object.entries(context)) {
        ctx[key] = value;
      }
    }
    return ctx;
  }
  async init() {}
  async dispose() {}
}

class ReferenceTestingClient implements TestingClient {
  readonly OS = "web";
  readonly arch = "paper";

  private browser: Browser | null = null;
  private page: Page | null = null;

  async eval<C = Ctx, R extends JSONValue = JSONValue>(
    fn: (ctx: GPUContext & C) => R | Promise<R>,
    ctx?: C,
  ): Promise<R> {
    if (!this.page) {
      throw new Error("RemoteSurface not initialized");
    }
    const source = `(async function Main(){
      const { device, adapter, gpu, cubeVertexArray, triangleVertWGSL, redFragWGSL } = window;
      class DrawingContext {
        constructor(device, width, height) {
            this.device = device;
            this.width = width;
            this.height = height;
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
        getImageData() {
            const commandEncoder = this.device.createCommandEncoder();
            const bytesPerRow = this.width * 4;
            commandEncoder.copyTextureToBuffer({ texture: this.texture }, { buffer: this.buffer, bytesPerRow }, [this.width, this.height]);
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
                format: gpu.getPreferredCanvasFormat(),
              };
            });
        }
    }
      const ctx = new DrawingContext(device, 1024, 1024);
      return (${fn.toString()})({
        device, adapter, gpu, 
        GPUBufferUsage,
        GPUColorWrite,
        GPUMapMode,
        GPUShaderStage,
        GPUTextureUsage,
        cubeVertexArray,
        triangleVertWGSL,
        redFragWGSL,
        ctx,
        ...${JSON.stringify(ctx || {})}
      });
    })();`;
    const data = await this.page.evaluate(source);
    return data as R;
  }

  async init() {
    const browser = await puppeteer.launch({
      headless: !DEBUG,
      args: ["--enable-unsafe-webgpu"],
    });
    const page = await browser.newPage();
    page.on("console", (msg) => console.log(msg.text()));
    page.on("pageerror", (error) => {
      console.error(error.message);
    });
    await page
      .goto("chrome://gpu", {
        waitUntil: "networkidle0",
        timeout: 20 * 60 * 1000,
      })
      .catch((e) => console.log(e));
    await page.waitForNetworkIdle();
    await page.evaluate(
      `
(async () => {
  window.gpu = navigator.gpu;
  if (!gpu) {
    const canvas = document.createElement('canvas');
    const gl = canvas.getContext('webgl');
    throw new Error("WebGPU is not available. WebGL: " + !!(gl && gl instanceof WebGLRenderingContext));
  }
  window.adapter = await gpu.requestAdapter();
  if (!adapter) {
    throw new Error("No adapter");
  }
  window.device = await adapter.requestDevice();
  window.cubeVertexArray = new Float32Array(${JSON.stringify(Array.from(cubeVertexArray))});
  window.triangleVertWGSL = \`${triangleVertWGSL}\`;
  window.redFragWGSL = \`${redFragWGSL}\`;

})();
      `,
    );
    this.browser = browser;
    this.page = page;
  }
  async dispose() {
    if (this.browser && !DEBUG) {
      this.browser.close();
    }
  }
}

interface Bitmap {
  data: number[];
  width: number;
  height: number;
  format: string;
}

export const encodeImage = (bitmap: Bitmap) => {
  const { width, height, format } = bitmap;
  let data = new Uint8Array(bitmap.data);
  // Convert BGRA to RGBA if necessary
  if (format === "bgra8unorm") {
    data = new Uint8Array(bitmap.data.length);
    for (let i = 0; i < bitmap.data.length; i += 4) {
      data[i] = bitmap.data[i + 2]; // R
      data[i + 1] = bitmap.data[i + 1]; // G
      data[i + 2] = bitmap.data[i]; // B
      data[i + 3] = bitmap.data[i + 3]; // A
    }
  } else if (format !== "rgba8unorm") {
    throw new Error(`Unsupported format ${format}`);
  }
  // Create a new PNG
  const png = new PNG({
    width: width,
    height: height,
    filterType: -1,
  });
  png.data = Buffer.from(data);
  return png;
};

interface CheckImageOptions {
  maxPixelDiff?: number;
  threshold?: number;
  overwrite?: boolean;
  mute?: boolean;
  shouldFail?: boolean;
}

// On Github Action, the image decoding is slightly different
// all tests that show the oslo.jpg have small differences but look ok
const defaultCheckImageOptions = {
  maxPixelDiff: 200,
  threshold: 0.1,
  overwrite: false,
  mute: false,
  shouldFail: false,
};

export const checkImage = (
  toTest: PNG,
  relPath: string,
  opts?: CheckImageOptions,
) => {
  const options = { ...defaultCheckImageOptions, ...opts };
  const { overwrite, threshold, mute, maxPixelDiff, shouldFail } = options;
  const p = path.resolve(__dirname, relPath);
  if (fs.existsSync(p) && !overwrite) {
    const ref = fs.readFileSync(p);
    const baseline = PNG.sync.read(ref);
    const diffImage = new PNG({
      width: baseline.width,
      height: baseline.height,
    });
    if (baseline.width !== toTest.width || baseline.height !== toTest.height) {
      throw new Error(
        `Image sizes don't match: ${baseline.width}x${baseline.height} vs ${toTest.width}x${toTest.height}`,
      );
    }
    const diffPixelsCount = pixelmatch(
      baseline.data,
      toTest.data,
      diffImage.data,
      baseline.width,
      baseline.height,
      { threshold },
    );
    if (!mute) {
      if (diffPixelsCount > maxPixelDiff && !shouldFail) {
        console.log(`${p} didn't match`);
        fs.writeFileSync(`${p}.test.png`, PNG.sync.write(toTest));
        fs.writeFileSync(`${p}-diff-test.png`, PNG.sync.write(diffImage));
      }
      if (shouldFail) {
        expect(diffPixelsCount).not.toBeLessThanOrEqual(maxPixelDiff);
      } else {
        expect(diffPixelsCount).toBeLessThanOrEqual(maxPixelDiff);
      }
    }
    return diffPixelsCount;
  } else {
    const buffer = PNG.sync.write(toTest);
    fs.writeFileSync(p, buffer);
  }
  return 0;
};
