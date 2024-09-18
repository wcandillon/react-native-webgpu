import * as THREE from "three";
import { Image } from "react-native";
import { useEffect, useState } from "react";
import { GLTFLoader } from "GLTFLoader";
import { DRACOLoader } from "DRACOLoader";
import { RGBELoader } from "RGBELoader";

export const resolveAsset = (mod: ReturnType<typeof require>) => {
  return Image.resolveAssetSource(mod).uri;
};

export const debugManager = new THREE.LoadingManager();

debugManager.onStart = function (url, itemsLoaded, itemsTotal) {
  console.log(
    "Started loading file: " +
      url +
      ".\nLoaded " +
      itemsLoaded +
      " of " +
      itemsTotal +
      " files.",
  );
};

debugManager.onProgress = function (url, itemsLoaded, itemsTotal) {
  console.log(
    "Loading file: " +
      url +
      ".\nLoaded " +
      itemsLoaded +
      " of " +
      itemsTotal +
      " files.",
  );
};

debugManager.onError = function (url) {
  console.error("There was an error loading " + url);
};

export const useRGBE = (asset: ReturnType<typeof require>) => {
  const url = resolveAsset(asset);
  const [texture, setTexture] = useState<THREE.Texture | null>(null);
  useEffect(() => {
    const loader = new RGBELoader();
    loader.load(url, function (tex: THREE.Texture) {
      setTexture(tex);
    });
  }, [url]);
  return texture;
};

export const useGLTF = (asset: string) => {
  const [scene, setScene] = useState<THREE.Scene | null>(null);
  const url = resolveAsset(asset);
  useEffect(() => {
    const loader = new GLTFLoader();
    const dracoLoader = new DRACOLoader();
    loader.setDRACOLoader(dracoLoader);
    loader.load(url, function (data: { scene: THREE.Scene }) {
      setScene(data.scene);
    });
  }, [url]);
  return scene;
};
