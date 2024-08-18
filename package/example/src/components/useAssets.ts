/* eslint-disable @typescript-eslint/no-var-requires */
import { useEffect, useState } from "react";
import { Image } from "react-native";
import { Skia } from "@shopify/react-native-skia";
import "react-native-wgpu";

export const decodeImage = async (mod: number) => {
  const { uri } = Image.resolveAssetSource(mod);
  const data = await Skia.Data.fromURI(uri);
  const png = Skia.Image.MakeImageFromEncoded(data)!;

  const bitmap = {
    data: new Uint8ClampedArray(png.readPixels() as Uint8Array),
    width: png.width(),
    height: png.height(),
    format: "rgba8unorm",
  };

  return bitmap;
};

export interface AssetProps {
  assets: {
    di3D: ImageData;
    saturn: ImageData;
    moon: ImageData;
    react: ImageData;
  };
}

const useImageData = (mod: number) => {
  const [imageData, setImageData] = useState<ImageData | null>(null);
  useEffect(() => {
    (async () => {
      const { data, width, height } = await decodeImage(mod);
      setImageData({
        data,
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
  const react = useImageData(require("../assets/react.png"));
  if (!di3D || !moon || !saturn || !react) {
    return null;
  }
  return {
    di3D,
    moon,
    saturn,
    react,
  };
};
