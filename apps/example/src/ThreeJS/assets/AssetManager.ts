/* eslint-disable @typescript-eslint/no-explicit-any */
import * as THREE from "three";
import { Image } from "react-native";
import { useEffect, useState } from "react";
import { GLTFLoader } from "three/addons/loaders/GLTFLoader";
import { DRACOLoader } from "three/addons/loaders/DRACOLoader";
import { RGBELoader } from "three/addons/loaders/RGBELoader";

export interface GLTF {
  animations: THREE.AnimationClip[];
  scene: THREE.Group;
  scenes: THREE.Group[];
  cameras: THREE.Camera[];
  asset: {
    copyright?: string | undefined;
    generator?: string | undefined;
    version?: string | undefined;
    minVersion?: string | undefined;
    extensions?: any;
    extras?: any;
  };
  parser: any;
  userData: Record<string, any>;
}

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

export const useGeometry = (uri: string) => {
  const [geometry, setGeometry] = useState<THREE.BufferGeometry | null>(null);
  useEffect(() => {
    const loader = new THREE.BufferGeometryLoader();
    loader.load(uri, function (geo) {
      setGeometry(geo);
    });
  }, [uri]);
  return geometry;
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

export const useGLTF = (asset: ReturnType<typeof require>) => {
  const [GLTF, setGLTF] = useState<GLTF | null>(null);
  const url = resolveAsset(asset);
  useEffect(() => {
    const loader = new GLTFLoader(debugManager);
    const dracoLoader = new DRACOLoader();
    loader.setDRACOLoader(dracoLoader);
    loader.load(url, (model: GLTF) => {
      setGLTF(model);
    });
  }, [url]);
  return GLTF;
};
