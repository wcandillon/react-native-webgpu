import { vec3 as vec3Type, mat4 as mat4Type } from "wgpu-matrix";

declare global {
  var vec3: typeof vec3Type;
  var mat4: typeof mat4Type;
}

global.vec3 = vec3;
global.mat4 = mat4;
