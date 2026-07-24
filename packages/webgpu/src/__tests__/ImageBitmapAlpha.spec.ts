import { PNG } from "pngjs";

import { client } from "./setup";

const straightRows = [
  [128, 128, 128, 128],
  [17, 34, 51, 64],
];
const premultipliedRows = [
  [64, 64, 64, 128],
  [4, 9, 13, 64],
];
const unpremultipliedRows = [
  [128, 128, 128, 128],
  [16, 36, 52, 64],
];

const png = new PNG({ width: 1, height: 2 });
png.data = Buffer.from(straightRows.flat());
const pngBase64 = PNG.sync.write(png).toString("base64");

const cases = (["none", "premultiply"] as const).flatMap((sourceAlpha) =>
  (["omitted", false, true] as const).flatMap((destinationAlpha) =>
    ([false, true] as const).map((flipY) => ({
      sourceAlpha,
      destinationAlpha,
      flipY,
    })),
  ),
);

describe("ImageBitmap alpha representation", () => {
  it.each(cases)(
    "copies source=$sourceAlpha destination=$destinationAlpha flipY=$flipY",
    async ({ sourceAlpha, destinationAlpha, flipY }) => {
      if (client.OS !== "ios") {
        return;
      }

      const result = await client.eval(
        ({
          device,
          pngBase64: encodedPng,
          sourceAlpha: sourceRepresentation,
          destinationAlpha: destinationRepresentation,
          flipY: shouldFlip,
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
                usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC,
              });
              const destination =
                destinationRepresentation === "omitted"
                  ? { texture }
                  : {
                      texture,
                      premultipliedAlpha: destinationRepresentation,
                    };

              device.queue.copyExternalImageToTexture(
                { source: bitmap, flipY: shouldFlip },
                destination,
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
          pngBase64,
          sourceAlpha,
          destinationAlpha,
          flipY,
        },
      );

      let expected = unpremultipliedRows;
      if (destinationAlpha === true) {
        expected = premultipliedRows;
      } else if (sourceAlpha === "none") {
        expected = straightRows;
      }
      expect(result).toEqual(flipY ? [...expected].reverse() : expected);
    },
  );
});
