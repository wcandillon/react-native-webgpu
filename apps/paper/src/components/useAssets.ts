import { useEffect, useState } from "react";
import { Image } from "react-native";
import "react-native-wgpu";

export const fetchAsset = async (mod: number) => {
  const response = await fetch(Image.resolveAssetSource(mod).uri);
  return response;
};

export const decodeImage = async (mod: number) => {
  const { uri } = Image.resolveAssetSource(mod);
  const response = await fetch(uri);
  const blob = await response.blob();
  const image = await createImageBitmap(blob);
  return image;
};

export interface AssetProps {
  assets: {
    di3D: ImageBitmap;
    saturn: ImageBitmap;
    moon: ImageBitmap;
    react: ImageBitmap;
  };
}

const useImageData = (mod: number) => {
  const [imageData, setImageData] = useState<ImageBitmap | null>(null);
  useEffect(() => {
    (async () => {
      setImageData(await decodeImage(mod));
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
