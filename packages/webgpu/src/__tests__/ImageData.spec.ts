import fs from "fs";
import path from "path";

import {
  checkImage,
  client,
  encodeImage,
  decodeImage,
  itSkipsOnWeb,
} from "./setup";

describe("Image Bitmap", () => {
  it("createImageBitmap (1)", async () => {
    const bitmap = await decodeImage(
      path.join(__dirname, "./assets/Di-3d.png"),
    );
    const image = encodeImage(bitmap);
    checkImage(image, "snapshots/ref.png");
  });
  it("createImageBitmap (2)", async () => {
    const bitmap = await decodeImage(
      path.join(__dirname, "./assets/Di-3d.png"),
    );
    const result = await client.eval(
      ({ bitmap: bmp }) => {
        return bmp;
      },
      { bitmap },
    );
    const image = encodeImage(result);
    checkImage(image, "snapshots/ref.png");
  });
  // The following tests exercise the React Native ArrayBuffer/TypedArray
  // overload of createImageBitmap, which is not part of the standard web API.
  itSkipsOnWeb("createImageBitmap from ArrayBuffer", async () => {
    const pngBytes = Array.from(
      fs.readFileSync(path.join(__dirname, "./assets/Di-3d.png")),
    );
    const expected = decodeImage(path.join(__dirname, "./assets/Di-3d.png"));
    const result = await client.eval(
      ({ pngData }) => {
        const bytes = new Uint8Array(pngData);
        return createImageBitmap(bytes.buffer).then((bmp) => {
          return { width: bmp.width, height: bmp.height };
        });
      },
      { pngData: pngBytes },
    );
    expect(result.width).toBe(expected.width);
    expect(result.height).toBe(expected.height);
  });
  itSkipsOnWeb("createImageBitmap from Uint8Array", async () => {
    const pngBytes = Array.from(
      fs.readFileSync(path.join(__dirname, "./assets/Di-3d.png")),
    );
    const expected = decodeImage(path.join(__dirname, "./assets/Di-3d.png"));
    const result = await client.eval(
      ({ pngData }) => {
        const bytes = new Uint8Array(pngData);
        return createImageBitmap(bytes).then((bmp) => {
          return { width: bmp.width, height: bmp.height };
        });
      },
      { pngData: pngBytes },
    );
    expect(result.width).toBe(expected.width);
    expect(result.height).toBe(expected.height);
  });
  itSkipsOnWeb(
    "createImageBitmap from Uint8Array subarray (byteOffset/byteLength)",
    async () => {
      const pngBytes = Array.from(
        fs.readFileSync(path.join(__dirname, "./assets/Di-3d.png")),
      );
      const expected = decodeImage(path.join(__dirname, "./assets/Di-3d.png"));
      const result = await client.eval(
        ({ pngData }) => {
          // Embed PNG bytes at an offset within a larger buffer
          const padding = 128;
          const totalLength = padding + pngData.length + padding;
          const largeBuffer = new ArrayBuffer(totalLength);
          const fullView = new Uint8Array(largeBuffer);
          // Fill with garbage bytes
          fullView.fill(0xff);
          // Copy PNG bytes into the middle
          const pngView = new Uint8Array(largeBuffer, padding, pngData.length);
          for (let i = 0; i < pngData.length; i++) {
            pngView[i] = pngData[i];
          }
          // createImageBitmap must respect byteOffset/byteLength of the view,
          // not use the full underlying ArrayBuffer (which has garbage padding)
          return createImageBitmap(pngView).then((bmp) => {
            return { width: bmp.width, height: bmp.height };
          });
        },
        { pngData: pngBytes },
      );
      expect(result.width).toBe(expected.width);
      expect(result.height).toBe(expected.height);
    },
  );
});
