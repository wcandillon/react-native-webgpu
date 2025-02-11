import { vec3, mat4 } from "wgpu-matrix";

declare global {
  var wgpuMatrix: {
    vec3: typeof vec3;
    mat4: typeof mat4;
  };
}

global.wgpuMatrix = {
  vec3,
  mat4,
};
