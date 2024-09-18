import * as THREE from "three";
import { Image } from "react-native";

export const resolveAsset = (mod: ReturnType<typeof require>) => {
  return Image.resolveAssetSource(mod).uri;
};

export const debugManager = new THREE.LoadingManager();
debugManager.setURLModifier((url) => {
  return url;
});

debugManager.onError = function (url) {
  console.log("There was an error loading " + url);
};
