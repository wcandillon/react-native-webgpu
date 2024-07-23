/* eslint-disable @typescript-eslint/no-var-requires */
import { useEffect, useState } from "react";
import { Image } from "react-native";

const useImageData = (mod: number) => {
  const [imageData, setImageData] = useState<ImageData | null>(null);
  useEffect(() => {
    (async () => {
      const { uri, width, height } = Image.resolveAssetSource(mod);
      const resp = await fetch(uri);
      const buffer = await resp.arrayBuffer();
      const data = new Uint8ClampedArray(buffer);
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
  if (!di3D) {
    return null;
  }
  return {
    di3D,
  };
};
