/* eslint-disable @typescript-eslint/no-var-requires */
import { useEffect, useState } from "react";
import { Image } from "react-native";
import { Skia } from "@shopify/react-native-skia";

export const decodeImage = async (uri: string) => {
  const data = await Skia.Data.fromURI(uri);
  const png = Skia.Image.MakeImageFromEncoded(data)!;

  const bitmap = {
    data: Array.from(png.readPixels() as Uint8Array),
    width: png.width(),
    height: png.height(),
    format: "rgba8unorm",
  };

  return bitmap;
};

const useImageData = (mod: number) => {
  const [imageData, setImageData] = useState<ImageData | null>(null);
  useEffect(() => {
    (async () => {
      const { uri } = Image.resolveAssetSource(mod);
      const { data, width, height } = await decodeImage(uri);
      setImageData({
        data: new Uint8ClampedArray(data),
        width,
        height,
        colorSpace: "srgb",
      });
    })();
  }, [mod]);
  return imageData;
};

export const useAssets = () => {
  const di3D = useImageData(require("../assets/Di-3d.png"));
  const moon = useImageData(require("../assets/moon.png"));
  const saturn = useImageData(require("../assets/saturn.png"));
  if (!di3D || !moon || !saturn) {
    return null;
  }
  return {
    di3D,
    moon,
    saturn,
  };
};
