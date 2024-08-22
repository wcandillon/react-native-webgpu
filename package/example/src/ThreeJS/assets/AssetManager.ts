/* eslint-disable @typescript-eslint/no-var-requires */
import * as THREE from "three/webgpu";
import { Image } from "react-native";

const resolve = (mod: ReturnType<typeof require>) => {
  return Image.resolveAssetSource(mod).uri;
};

const urls = {
  "textures/equirectangular/royal_esplanade_1k.hdr": resolve(
    require("./royal_esplanade_1k.hdr"),
  ),
  "models/gltf/DamagedHelmet/glTF/DamagedHelmet.gltf": resolve(
    require("./DamagedHelmet.gltf"),
  ),
  "models/gltf/DamagedHelmet/glTF/./DamagedHelmet.bin": resolve(
    require("./DamagedHelmet.bin"),
  ),
  "models/gltf/DamagedHelmet/glTF/./Default_albedo.jpg": resolve(
    require("./Default_albedo.jpg"),
  ),
  "models/gltf/DamagedHelmet/glTF/./Default_metalRoughness.jpg": resolve(
    require("./Default_metalRoughness.jpg"),
  ),
  "models/gltf/DamagedHelmet/glTF/./Default_normal.jpg": resolve(
    require("./Default_normal.jpg"),
  ),
  "models/gltf/DamagedHelmet/glTF/./Default_AO.jpg": resolve(
    require("./Default_AO.jpg"),
  ),
  "models/gltf/DamagedHelmet/glTF/./Default_emissive.jpg": resolve(
    require("./Default_emissive.jpg"),
  ),
};

export const manager = new THREE.LoadingManager();
manager.setURLModifier((url) => {
  if (urls[url]) {
    return urls[url];
  }
  return url;
});

manager.onError = function (url) {
  console.log("There was an error loading " + url);
};
