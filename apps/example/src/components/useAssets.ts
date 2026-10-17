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

type ImageLoad =
  | { readonly status: "loading" }
  | { readonly status: "loaded"; readonly image: ImageBitmap }
  | { readonly status: "failed"; readonly error: unknown };

const useImageData = (mod: number) => {
  const [load, setLoad] = useState<ImageLoad>({ status: "loading" });
  useEffect(() => {
    let active = true;
    decodeImage(mod).then(
      (image) => {
        if (active) {
          setLoad({ status: "loaded", image });
        }
      },
      (error: unknown) => {
        if (active) {
          setLoad({ status: "failed", error });
        }
      },
    );
    return () => {
      active = false;
    };
  }, [mod]);
  // Thrown while rendering, so a failed load reaches the error screen instead of a blank one.
  if (load.status === "failed") {
    throw load.error;
  }
  return load.status === "loaded" ? load.image : null;
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
