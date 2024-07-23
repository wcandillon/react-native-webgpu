import path from "path";

import { checkImage, client, encodeImage, decodeImage } from "./setup";

describe("Image Bitmap", () => {
  it("createImageBitmap (1)", async () => {
    const bitmap = await decodeImage(
      path.join(__dirname, "../../example/src/assets/Di-3d.png"),
    );
    const image = encodeImage(bitmap);
    checkImage(image, "snapshots/ref.png");
  });
  it("createImageBitmap (2)", async () => {
    const bitmap = await decodeImage(
      path.join(__dirname, "../../example/src/assets/Di-3d.png"),
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
});
