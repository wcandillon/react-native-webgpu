import { vec3, mat4 } from "wgpu-matrix";

// eslint-disable-next-line @typescript-eslint/ban-ts-comment
// @ts-expect-error
global.vec3 = vec3;
// eslint-disable-next-line @typescript-eslint/ban-ts-comment
// @ts-expect-error
global.mat4 = mat4;
