import fs from "fs";
import path from "path";

import { PNG } from "pngjs";

import { checkImage, client, encodeImage, fixtureUrl } from "./setup";

// A 32x32 gradient with a vertical alpha ramp (row 0 fully transparent, last
// row fully opaque) and a horizontal color ramp, so alpha-mode mistakes and
// vertical flips both produce large snapshot diffs. The baselines are computed
// with the same integer arithmetic as ImageBitmap::convertAlpha; pixelmatch
// tolerance absorbs the platform decoders' off-by-one rounding differences
// (iOS and Android both decode premultiplied, so "none" is a lossy round trip
// there).
const assetPath = path.resolve(__dirname, "./assets/alpha-gradient.png");

type SourceAlpha = "none" | "premultiply" | "default" | "omitted";
type DestinationAlpha = boolean | "omitted";

interface BitmapResult {
  data: number[];
  width: number;
  height: number;
  format: string;
}

const flipRows = (bitmap: BitmapResult): BitmapResult => {
  const rowSize = bitmap.width * 4;
  const data: number[] = [];
  for (let y = bitmap.height - 1; y >= 0; y--) {
    data.push(...bitmap.data.slice(y * rowSize, (y + 1) * rowSize));
  }
  return { ...bitmap, data };
};

const snapshotFor = (
  sourceAlpha: SourceAlpha,
  destinationAlpha: DestinationAlpha,
) => {
  if (destinationAlpha === true) {
    return "snapshots/image-bitmap-alpha-premultiplied.png";
  }
  if (sourceAlpha === "none") {
    return "snapshots/image-bitmap-alpha-straight.png";
  }
  // "premultiply", "default", and omitted options all store premultiplied
  // data, so copying to a straight-alpha destination is a lossy round trip.
  return "snapshots/image-bitmap-alpha-roundtrip.png";
};

interface BlobCase {
  sourceAlpha: SourceAlpha;
  destinationAlpha: DestinationAlpha;
  flipY: boolean;
}

const blobCases: BlobCase[] = (
  ["none", "premultiply", "omitted"] as const
).flatMap((sourceAlpha) =>
  (["omitted", false, true] as const).flatMap((destinationAlpha) =>
    ([false, true] as const).map((flipY) => ({
      sourceAlpha,
      destinationAlpha,
      flipY,
    })),
  ),
);
// premultiplyAlpha: "default" behaves like "premultiply"; one case is enough.
blobCases.push({
  sourceAlpha: "default",
  destinationAlpha: false,
  flipY: false,
});

describe("ImageBitmap alpha representation", () => {
  it.each(blobCases)(
    "blob source=$sourceAlpha destination=$destinationAlpha flipY=$flipY",
    async ({ sourceAlpha, destinationAlpha, flipY }) => {
      const result = await client.eval(
        ({
          device,
          url,
          sourceAlpha: sourceRepresentation,
          destinationAlpha: destinationRepresentation,
          flipY: shouldFlip,
        }) => {
          return fetch(url)
            .then((response) => response.blob())
            .then((blob) =>
              createImageBitmap(
                blob,
                sourceRepresentation === "omitted"
                  ? undefined
                  : { premultiplyAlpha: sourceRepresentation },
              ),
            )
            .then((bitmap) => {
              const { width } = bitmap;
              const { height } = bitmap;
              const texture = device.createTexture({
                size: [width, height],
                format: "rgba8unorm",
                usage:
                  GPUTextureUsage.COPY_DST |
                  GPUTextureUsage.COPY_SRC |
                  GPUTextureUsage.RENDER_ATTACHMENT,
              });
              device.queue.copyExternalImageToTexture(
                { source: bitmap, flipY: shouldFlip },
                destinationRepresentation === "omitted"
                  ? { texture }
                  : { texture, premultipliedAlpha: destinationRepresentation },
                [width, height],
              );

              const bytesPerRow = 256;
              const output = device.createBuffer({
                size: bytesPerRow * height,
                usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
              });
              const encoder = device.createCommandEncoder();
              encoder.copyTextureToBuffer(
                { texture },
                { buffer: output, bytesPerRow },
                [width, height],
              );
              device.queue.submit([encoder.finish()]);

              return output.mapAsync(GPUMapMode.READ).then(() => {
                const mapped = new Uint8Array(output.getMappedRange());
                const data: number[] = [];
                for (let y = 0; y < height; y++) {
                  for (let i = 0; i < width * 4; i++) {
                    data.push(mapped[y * bytesPerRow + i]);
                  }
                }
                output.unmap();
                output.destroy();
                texture.destroy();
                bitmap.close();
                return { data, width, height, format: "rgba8unorm" };
              });
            });
        },
        {
          url: fixtureUrl("alpha-gradient.png"),
          sourceAlpha,
          destinationAlpha,
          flipY,
        },
      );

      const upright = flipY ? flipRows(result) : result;
      checkImage(
        encodeImage(upright),
        snapshotFor(sourceAlpha, destinationAlpha),
      );
    },
  );

  // The React Native ArrayBuffer overload of createImageBitmap goes through
  // createImageBitmapFromDataAsync, a separate native path from the blob one.
  // It is not part of the standard web API, so the reference client skips it.
  const bufferCases: {
    sourceAlpha: "none" | "premultiply";
    destinationAlpha: boolean;
  }[] = (["none", "premultiply"] as const).flatMap((sourceAlpha) =>
    ([false, true] as const).map((destinationAlpha) => ({
      sourceAlpha,
      destinationAlpha,
    })),
  );
  it.each(bufferCases)(
    "buffer source=$sourceAlpha destination=$destinationAlpha",
    async ({ sourceAlpha, destinationAlpha }) => {
      if (client.OS === "web") {
        return;
      }
      const pngData = Array.from(fs.readFileSync(assetPath));
      const result = await client.eval(
        ({
          device,
          pngData: encodedPng,
          sourceAlpha: sourceRepresentation,
          destinationAlpha: destinationRepresentation,
        }) => {
          const bytes = new Uint8Array(encodedPng);
          return createImageBitmap(bytes.buffer, {
            premultiplyAlpha: sourceRepresentation,
          }).then((bitmap) => {
            const { width } = bitmap;
            const { height } = bitmap;
            const texture = device.createTexture({
              size: [width, height],
              format: "rgba8unorm",
              usage:
                GPUTextureUsage.COPY_DST |
                GPUTextureUsage.COPY_SRC |
                GPUTextureUsage.RENDER_ATTACHMENT,
            });
            device.queue.copyExternalImageToTexture(
              { source: bitmap },
              { texture, premultipliedAlpha: destinationRepresentation },
              [width, height],
            );

            const bytesPerRow = 256;
            const output = device.createBuffer({
              size: bytesPerRow * height,
              usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
            });
            const encoder = device.createCommandEncoder();
            encoder.copyTextureToBuffer(
              { texture },
              { buffer: output, bytesPerRow },
              [width, height],
            );
            device.queue.submit([encoder.finish()]);

            return output.mapAsync(GPUMapMode.READ).then(() => {
              const mapped = new Uint8Array(output.getMappedRange());
              const data: number[] = [];
              for (let y = 0; y < height; y++) {
                for (let i = 0; i < width * 4; i++) {
                  data.push(mapped[y * bytesPerRow + i]);
                }
              }
              output.unmap();
              output.destroy();
              texture.destroy();
              bitmap.close();
              return { data, width, height, format: "rgba8unorm" };
            });
          });
        },
        {
          pngData,
          sourceAlpha,
          destinationAlpha,
        },
      );

      checkImage(
        encodeImage(result),
        snapshotFor(sourceAlpha, destinationAlpha),
      );
    },
  );

  // Exact-value check that conversions use round-to-nearest integer math.
  // Runs only on the node client, whose polyfill mirrors the C++ convertAlpha
  // arithmetic bit-for-bit. iOS and Android both decode premultiplied through
  // the platform imaging stack, so premultiplyAlpha "none" is a lossy round
  // trip on device; the reference browser's rounding may also differ by one.
  // Those platforms are covered by the tolerance-based snapshot matrix above.
  // Being node-only, this case can build its fixture at runtime and pass it as
  // a data: URI rather than going through fixtureUrl.
  const straightRows = [
    [128, 128, 128, 128],
    [17, 34, 51, 64],
  ];
  const premultipliedRows = [
    [64, 64, 64, 128],
    [4, 9, 13, 64],
  ];
  it.each([
    { sourceAlpha: "none", destinationAlpha: false, expected: straightRows },
    {
      sourceAlpha: "none",
      destinationAlpha: true,
      expected: premultipliedRows,
    },
  ] as const)(
    "preserves exact bytes for source=$sourceAlpha destination=$destinationAlpha",
    async ({ sourceAlpha, destinationAlpha, expected }) => {
      if (client.OS !== "node") {
        return;
      }
      const png = new PNG({ width: 1, height: 2 });
      png.data = Buffer.from(straightRows.flat());
      const fixtureBase64 = PNG.sync.write(png).toString("base64");

      const result = await client.eval(
        ({
          device,
          pngBase64: encodedPng,
          sourceAlpha: sourceRepresentation,
          destinationAlpha: destinationRepresentation,
        }) => {
          return fetch(`data:image/png;base64,${encodedPng}`)
            .then((response) => response.blob())
            .then((blob) =>
              createImageBitmap(blob, {
                premultiplyAlpha: sourceRepresentation,
              }),
            )
            .then((bitmap) => {
              const texture = device.createTexture({
                size: [1, 2],
                format: "rgba8unorm",
                usage:
                  GPUTextureUsage.COPY_DST |
                  GPUTextureUsage.COPY_SRC |
                  GPUTextureUsage.RENDER_ATTACHMENT,
              });
              device.queue.copyExternalImageToTexture(
                { source: bitmap },
                { texture, premultipliedAlpha: destinationRepresentation },
                [1, 2],
              );

              const bytesPerRow = 256;
              const output = device.createBuffer({
                size: bytesPerRow * 2,
                usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
              });
              const encoder = device.createCommandEncoder();
              encoder.copyTextureToBuffer(
                { texture },
                { buffer: output, bytesPerRow },
                [1, 2],
              );
              device.queue.submit([encoder.finish()]);

              return output.mapAsync(GPUMapMode.READ).then(() => {
                const data = new Uint8Array(output.getMappedRange());
                const rows = [
                  Array.from(data.slice(0, 4)),
                  Array.from(data.slice(bytesPerRow, bytesPerRow + 4)),
                ];
                output.unmap();
                output.destroy();
                texture.destroy();
                bitmap.close();
                return rows;
              });
            });
        },
        {
          pngBase64: fixtureBase64,
          sourceAlpha,
          destinationAlpha,
        },
      );

      expect(result).toEqual(expected);
    },
  );
});
