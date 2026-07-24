import fs from "fs";
import path from "path";

import { checkImage, client, encodeImage } from "./setup";

// createImageBitmap options beyond premultiplyAlpha (which is covered by
// ImageBitmapAlpha.spec.ts): the crop-rect overload, resizeWidth/resizeHeight,
// imageOrientation, and colorSpaceConversion.
//
// The native implementation currently ignores all of these, so this suite has
// a dual role:
// - On the reference clients (Chrome via test:ref, dawn.node via test:node)
//   it asserts the spec behavior. The snapshots were written by Chrome, and
//   the node polyfill independently reproduces them (crop and flip exactly,
//   resize within pixelmatch tolerance).
// - On iOS and Android it asserts the current behavior: the option is ignored
//   and the full, upright, unresized image comes through. When one of these
//   options gets implemented natively, the corresponding case fails here and
//   should be flipped over to the reference snapshot.
const assetPath = path.resolve(__dirname, "./assets/opaque-gradient.png");
const pngBase64 = fs.readFileSync(assetPath).toString("base64");
const p3AssetPath = path.resolve(__dirname, "./assets/p3-gradient.png");
const p3Base64 = fs.readFileSync(p3AssetPath).toString("base64");

const identitySnapshot = "assets/opaque-gradient.png";

interface OptionsCase {
  name: string;
  cropRect?: [number, number, number, number];
  options?: ImageBitmapOptions;
  referenceSnapshot: string;
}

const cases: OptionsCase[] = [
  {
    name: "crop",
    cropRect: [8, 4, 16, 24],
    referenceSnapshot: "snapshots/image-bitmap-options-crop.png",
  },
  {
    name: "resize down",
    options: { resizeWidth: 16, resizeHeight: 16 },
    referenceSnapshot: "snapshots/image-bitmap-options-resize-down.png",
  },
  {
    name: "resize up",
    options: { resizeWidth: 64, resizeHeight: 48 },
    referenceSnapshot: "snapshots/image-bitmap-options-resize-up.png",
  },
  {
    name: "crop and resize",
    cropRect: [8, 4, 16, 24],
    options: { resizeWidth: 32, resizeHeight: 32 },
    referenceSnapshot: "snapshots/image-bitmap-options-crop-resize.png",
  },
  {
    name: "imageOrientation flipY",
    options: { imageOrientation: "flipY" },
    referenceSnapshot: "snapshots/image-bitmap-options-flip.png",
  },
  {
    // No EXIF data in a PNG, so "from-image" must behave like the identity.
    name: "imageOrientation from-image",
    options: { imageOrientation: "from-image" },
    referenceSnapshot: identitySnapshot,
  },
];

const runCase = (
  encodedPng: string,
  cropRect: [number, number, number, number] | null,
  options: ImageBitmapOptions | null,
) =>
  client.eval(
    ({ device, pngBase64: png, cropRect: rect, options: bitmapOptions }) => {
      return fetch(`data:image/png;base64,${png}`)
        .then((response) => response.blob())
        .then((blob) =>
          rect === null
            ? createImageBitmap(blob, bitmapOptions ?? undefined)
            : createImageBitmap(
                blob,
                rect[0],
                rect[1],
                rect[2],
                rect[3],
                bitmapOptions ?? undefined,
              ),
        )
        .then((bitmap) => {
          const { width, height } = bitmap;
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
            { texture },
            [width, height],
          );

          const bytesPerRow = Math.ceil((width * 4) / 256) * 256;
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
      pngBase64: encodedPng,
      cropRect,
      options,
    },
  );

describe("createImageBitmap options", () => {
  it.each(cases)("$name", async ({ cropRect, options, referenceSnapshot }) => {
    const result = await runCase(pngBase64, cropRect ?? null, options ?? null);
    const isReference = client.OS === "web" || client.OS === "node";
    checkImage(
      encodeImage(result),
      isReference ? referenceSnapshot : identitySnapshot,
    );
  });

  // colorSpaceConversion on a Display-P3 tagged PNG (generated by Chrome, so
  // the profile chunk is exactly what a browser produces). "none" must return
  // the raw encoded values, which is also what pngjs decodes the asset to, so
  // the baseline is the asset itself. "default" converts P3 to sRGB and only
  // the browser implements that, so it runs against a Chrome-written snapshot
  // on the reference client only. Native behavior is currently inconsistent
  // between the straight-alpha (Core Image, unmanaged) and premultiplied
  // (CoreGraphics, device RGB) decode paths; these cases are skipped there
  // until that behavior is measured on-device and pinned down.
  it("colorSpaceConversion none returns raw pixel values", async () => {
    if (client.OS !== "web" && client.OS !== "node") {
      return;
    }
    const result = await runCase(p3Base64, null, {
      colorSpaceConversion: "none",
    });
    checkImage(encodeImage(result), "assets/p3-gradient.png");
  });

  it("colorSpaceConversion default converts to sRGB", async () => {
    if (client.OS !== "web") {
      return;
    }
    const result = await runCase(p3Base64, null, {
      colorSpaceConversion: "default",
    });
    checkImage(
      encodeImage(result),
      "snapshots/image-bitmap-options-p3-default.png",
    );
  });
});
