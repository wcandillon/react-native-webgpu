import { useEffect, useState } from "react";
import { Image, Platform } from "react-native";
import "react-native-webgpu";

import { android } from "../../app.json";

// react-native-test-app's application id: app.json's android.package, else its default.
const androidPackage =
  "package" in android && typeof android.package === "string"
    ? android.package
    : "com.microsoft.reacttestapp";

// Without Metro, an Android build names a bundled image by its drawable resource name, with no scheme.
export const resolveImageUrl = (mod: number) => {
  const { uri } = Image.resolveAssetSource(mod);
  return Platform.OS === "android" && !uri.includes(":")
    ? `android.resource://${androidPackage}/drawable/${uri}`
    : uri;
};

export const fetchAsset = async (mod: number) => {
  const response = await fetch(resolveImageUrl(mod));
  return response;
};

export const decodeImage = async (mod: number) => {
  const response = await fetch(resolveImageUrl(mod));
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
