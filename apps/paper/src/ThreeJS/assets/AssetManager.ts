import * as THREE from "three";
import { Image } from "react-native";

const resolve = (mod: ReturnType<typeof require>) => {
  return Image.resolveAssetSource(mod).uri;
};

const urls: Record<string, string> = {
  "models/json/suzanne_buffergeometry.json":
    "https://threejs.org/examples/models/json/suzanne_buffergeometry.json",
  "./textures/uv_grid_opengl.jpg": resolve(
    require("./textures/uv_grid_opengl.jpg"),
  ),
  "models/gltf/Michelle.glb": resolve(require("./michelle/model.gltf")),
  "models/gltf/model.bin": resolve(require("./michelle/model.bin")),
  "models/gltf/Ch03_1001_Diffuse.png": resolve(
    require("./michelle/Ch03_1001_Diffuse.png"),
  ),
  "models/gltf/Ch03_1001_Glossiness.png": resolve(
    require("./michelle/Ch03_1001_Glossiness.png"),
  ),
  "models/gltf/Ch03_1001_Normal.png": resolve(
    require("./michelle/Ch03_1001_Normal.png"),
  ),
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
  "jordan_shoe.glb": resolve(require("./jordan_shoe.glb")),
  "light.hdr": resolve(require("./light.hdr")),
};

export const manager = new THREE.LoadingManager();
manager.setURLModifier((url: string) => {
  const asset = urls[url];
  if (asset) {
    return asset;
  }
  console.error("url not found", url.substring(0, 150));
  return url;
});

manager.onStart = function (url, itemsLoaded, itemsTotal) {
  console.log(
    "Started loading file: " +
      url.substring(0, 150) +
      ".\nLoaded " +
      itemsLoaded +
      " of " +
      itemsTotal +
      " files.",
  );
};

manager.onLoad = function () {
  console.log("Loading complete!");
};

manager.onProgress = function (url, itemsLoaded, itemsTotal) {
  console.log(
    "Loading file: " +
      url.substring(0, 150) +
      ".\nLoaded " +
      itemsLoaded +
      " of " +
      itemsTotal +
      " files.",
  );
};

manager.onError = function (url) {
  console.log("There was an error loading " + url);
};
