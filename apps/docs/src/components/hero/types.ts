// Types for the hero shader gallery.

export interface HeroContext {
  device: GPUDevice;
  format: GPUTextureFormat;
  canvas: HTMLCanvasElement;
}

// Extra GPU resources a shader needs beyond the standard uniforms. These are
// bound at @group(1). `update` runs once per frame before drawing, e.g. to push
// shader-specific uniforms.
export interface ShaderResources {
  bindGroupLayout: GPUBindGroupLayout;
  bindGroup: GPUBindGroup;
  update?: (frame: number, timeSeconds: number) => void;
  destroy?: () => void;
}

export interface FragmentShader {
  kind: "fragment";
  // WGSL body. Must define `fn fs_main(@builtin(position) pos: vec4f) -> @location(0) vec4f`.
  // The standard uniforms (`u`) and a fullscreen `vs_main` are injected.
  code: string;
  resources?: (ctx: HeroContext) => ShaderResources;
}

export interface ComputeShader {
  kind: "compute";
  // WGSL body. Must define `@compute @workgroup_size(...) fn main(...)` and
  // write to the injected `screen` storage texture. The standard uniforms (`u`)
  // and `screen` binding are injected.
  code: string;
  workgroupSize: [number, number];
  resources?: (ctx: HeroContext) => ShaderResources;
}

// A shader written against the compute.toys runtime (multi-pass, persistent
// `pass_in`/`pass_out` buffers, `#storage` buffers, `time`/`mouse`/`custom`
// uniforms, the standard prelude/helpers, and `#define`/`#workgroup_count`/
// `#dispatch_*` directives). Run by the compat layer in ./computetoys.ts, which
// mirrors the binding model of the engine in apps/example.
export interface ComputeToysShader {
  kind: "computetoys";
  // Raw compute.toys source, directives included (as served by /view/<id>/wgsl).
  code: string;
  // `custom.*` uniform fields, in order, with default values (from
  // /view/<id>/json). Order only needs to be self-consistent.
  customOrder: string[];
  custom: Record<string, number>;
  // Whether pass buffers use rgba32float (compute.toys `float32Enabled`).
  passF32?: boolean;
  // Seconds for a full slow camera orbit driven through `mouse.pos` (0 = static).
  orbitPeriod?: number;
  // Cap the internal render resolution (longest side, device px) and upscale via
  // the blit. compute.toys shaders are tuned for a fixed modest resolution;
  // heavy per-pixel work at full DPR-scaled hero size can hang the GPU.
  // Defaults to 1024.
  maxDimension?: number;
}

export interface ShaderEntry {
  id: string;
  title: string;
  author: string;
  // Link to the original (compute.toys page, gist, repo, etc.). Optional for
  // first-party shaders.
  sourceUrl?: string;
  // SPDX-style identifier or short phrase. Credit is not a license: only ship
  // shaders that are explicitly permissively licensed or used with permission.
  license: string;
  // Whether the shader renders predominantly dark or light. Drives the hero
  // overlay (title/buttons/credit) contrast, independent of the site theme.
  // Defaults to "dark".
  appearance?: "dark" | "light";
  shader: FragmentShader | ComputeShader | ComputeToysShader;
}
