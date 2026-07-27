import { client, fixtureUrl } from "./setup";

interface OriginCase {
  name: string;
  fileName: string;
  origin: [number, number];
  copySize: [number, number];
  flipY: boolean;
  sourcePremultiplied: boolean;
}

const cases: OriginCase[] = [
  {
    name: "uses a non-zero horizontal origin",
    fileName: "opaque-gradient.png",
    origin: [8, 0],
    copySize: [16, 32],
    flipY: false,
    sourcePremultiplied: false,
  },
  {
    name: "uses a non-zero vertical origin",
    fileName: "opaque-gradient.png",
    origin: [0, 8],
    copySize: [32, 16],
    flipY: false,
    sourcePremultiplied: false,
  },
  {
    name: "combines origin with alpha conversion",
    fileName: "alpha-gradient.png",
    origin: [8, 4],
    copySize: [16, 20],
    flipY: false,
    sourcePremultiplied: true,
  },
  {
    name: "flips only the selected source region",
    fileName: "opaque-gradient.png",
    origin: [8, 4],
    copySize: [16, 20],
    flipY: true,
    sourcePremultiplied: false,
  },
  {
    name: "preserves the zero-origin path",
    fileName: "opaque-gradient.png",
    origin: [0, 0],
    copySize: [16, 16],
    flipY: false,
    sourcePremultiplied: false,
  },
];

describe("copyExternalImageToTexture source origin", () => {
  it.each(cases)("$name", async (testCase) => {
    const result = await client.eval(
      ({ device, url, origin, copySize, flipY, sourcePremultiplied }) => {
        return fetch(url)
          .then((response) => response.blob())
          .then((blob) =>
            createImageBitmap(blob, {
              premultiplyAlpha: sourcePremultiplied ? "premultiply" : "none",
            }),
          )
          .then((bitmap) => {
            const fullTexture = device.createTexture({
              size: [bitmap.width, bitmap.height],
              format: "rgba8unorm",
              usage:
                GPUTextureUsage.COPY_DST |
                GPUTextureUsage.COPY_SRC |
                GPUTextureUsage.RENDER_ATTACHMENT,
            });
            const regionTexture = device.createTexture({
              size: copySize,
              format: "rgba8unorm",
              usage:
                GPUTextureUsage.COPY_DST |
                GPUTextureUsage.COPY_SRC |
                GPUTextureUsage.RENDER_ATTACHMENT,
            });
            device.queue.copyExternalImageToTexture(
              { source: bitmap },
              { texture: fullTexture, premultipliedAlpha: false },
              [bitmap.width, bitmap.height],
            );
            device.queue.copyExternalImageToTexture(
              { source: bitmap, origin, flipY },
              { texture: regionTexture, premultipliedAlpha: false },
              copySize,
            );

            const bytesPerRow = 256;
            const fullOutput = device.createBuffer({
              size: bytesPerRow * bitmap.height,
              usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
            });
            const regionOutput = device.createBuffer({
              size: bytesPerRow * copySize[1],
              usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
            });
            const encoder = device.createCommandEncoder();
            encoder.copyTextureToBuffer(
              { texture: fullTexture },
              { buffer: fullOutput, bytesPerRow },
              [bitmap.width, bitmap.height],
            );
            encoder.copyTextureToBuffer(
              { texture: regionTexture },
              { buffer: regionOutput, bytesPerRow },
              copySize,
            );
            device.queue.submit([encoder.finish()]);

            return Promise.all([
              fullOutput.mapAsync(GPUMapMode.READ),
              regionOutput.mapAsync(GPUMapMode.READ),
            ]).then(() => {
              const readRows = (
                buffer: GPUBuffer,
                width: number,
                height: number,
              ) => {
                const mapped = new Uint8Array(buffer.getMappedRange());
                const data: number[] = [];
                for (let row = 0; row < height; row++) {
                  for (let column = 0; column < width * 4; column++) {
                    data.push(mapped[row * bytesPerRow + column]);
                  }
                }
                return data;
              };
              const full = readRows(fullOutput, bitmap.width, bitmap.height);
              const region = readRows(regionOutput, copySize[0], copySize[1]);
              const sourceWidth = bitmap.width;

              fullOutput.unmap();
              regionOutput.unmap();
              fullOutput.destroy();
              regionOutput.destroy();
              fullTexture.destroy();
              regionTexture.destroy();
              bitmap.close();

              return { full, region, sourceWidth };
            });
          });
      },
      {
        url: fixtureUrl(testCase.fileName),
        origin: testCase.origin,
        copySize: testCase.copySize,
        flipY: testCase.flipY,
        sourcePremultiplied: testCase.sourcePremultiplied,
      },
    );

    const expected: number[] = [];
    const [originX, originY] = testCase.origin;
    const [copyWidth, copyHeight] = testCase.copySize;
    for (let row = 0; row < copyHeight; row++) {
      const sourceRow = originY + (testCase.flipY ? copyHeight - 1 - row : row);
      const start = (sourceRow * result.sourceWidth + originX) * 4;
      expected.push(...result.full.slice(start, start + copyWidth * 4));
    }
    expect(result.region).toEqual(expected);
  });

  it.each([
    {
      name: "horizontal",
      origin: [17, 0] as [number, number],
      copySize: [16, 32] as [number, number],
    },
    {
      name: "vertical",
      origin: [0, 17] as [number, number],
      copySize: [32, 16] as [number, number],
    },
  ])("rejects an out-of-bounds $name region", async (testCase) => {
    const didThrow = await client.eval(
      ({ device, url, origin, copySize }) => {
        return fetch(url)
          .then((response) => response.blob())
          .then((blob) => createImageBitmap(blob))
          .then((bitmap) => {
            const texture = device.createTexture({
              size: copySize,
              format: "rgba8unorm",
              usage:
                GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT,
            });
            let threw = false;
            try {
              device.queue.copyExternalImageToTexture(
                { source: bitmap, origin },
                { texture },
                copySize,
              );
            } catch {
              threw = true;
            } finally {
              texture.destroy();
              bitmap.close();
            }
            return threw;
          });
      },
      {
        url: fixtureUrl("opaque-gradient.png"),
        origin: testCase.origin,
        copySize: testCase.copySize,
      },
    );

    expect(didThrow).toBe(true);
  });
});
