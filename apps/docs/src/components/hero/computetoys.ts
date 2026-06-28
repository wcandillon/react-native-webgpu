// compute.toys compatibility runtime.
//
// Mirrors the binding model of the engine in apps/example/src/ComputeToys so
// that real compute.toys shaders run unmodified. The fixed bind group (group 0)
// is:
//   0,1   storage1, storage2   (read_write storage buffers; #storage maps here)
//   2     time                 (uniform Time)
//   3     mouse                (uniform Mouse)
//   4     _keyboard            (uniform array<vec4<u32>,2>)
//   5     custom               (uniform Custom)
//   6     dispatch             (uniform DispatchInfo, dynamic offset)
//   7     screen               (storage texture rgba16float)
//   8     pass_in              (texture_2d_array<f32>)
//   9     pass_out             (storage texture array, rgba32float/rgba16float)
//   10,11 channel0, channel1   (texture_2d<f32>, blank 1x1 by default)
//   12-17 samplers             (nearest/bilinear/trilinear, clamp + repeat)
//
// Pass buffers are screen-sized, 4 layers, ping-ponged by copying pass_out into
// pass_in after every dispatch. Then the `screen` texture is blitted/upscaled to
// the swapchain.

import { BLIT_SHADER, SCREEN_FORMAT } from "./prelude";
import type { Pass } from "./runner";
import type { ComputeToysShader, HeroContext } from "./types";

const STORAGE_SIZE = 128 * 1024 * 1024; // 128MB, matches compute.toys
const MAX_CUSTOM_PARAMS = 32;
const OFFSET_ALIGNMENT = 256;
const PASS_LAYERS = 4;

// ---------------------------------------------------------------------------
// Prelude (aliases, structs, binding decls, helpers) — mirrors engine.getPrelude
// ---------------------------------------------------------------------------

function buildAliases(): string {
  let s = "";
  for (const [alias, type] of [
    ["int", "i32"],
    ["uint", "u32"],
    ["float", "f32"],
  ]) {
    s += `alias ${alias} = ${type};\n`;
  }
  for (const [alias, type] of [
    ["int", "i32"],
    ["uint", "u32"],
    ["float", "f32"],
    ["bool", "bool"],
  ]) {
    for (let n = 2; n < 5; n++) {
      s += `alias ${alias}${n} = vec${n}<${type}>;\n`;
    }
  }
  for (let n = 2; n < 5; n++) {
    for (let m = 2; m < 5; m++) {
      s += `alias float${n}x${m} = mat${n}x${m}<f32>;\n`;
    }
  }
  return s;
}

function buildBindingDecls(passF32: boolean): string {
  const passFormat = passF32 ? "rgba32float" : "rgba16float";
  return `
@group(0) @binding(2) var<uniform> time: Time;
@group(0) @binding(3) var<uniform> mouse: Mouse;
@group(0) @binding(4) var<uniform> _keyboard: array<vec4<u32>,2>;
@group(0) @binding(5) var<uniform> custom: Custom;
@group(0) @binding(6) var<uniform> dispatch: DispatchInfo;
@group(0) @binding(7) var screen: texture_storage_2d<${SCREEN_FORMAT},write>;
@group(0) @binding(8) var pass_in: texture_2d_array<f32>;
@group(0) @binding(9) var pass_out: texture_storage_2d_array<${passFormat},write>;
@group(0) @binding(10) var channel0: texture_2d<f32>;
@group(0) @binding(11) var channel1: texture_2d<f32>;
@group(0) @binding(12) var nearest: sampler;
@group(0) @binding(13) var bilinear: sampler;
@group(0) @binding(14) var trilinear: sampler;
@group(0) @binding(15) var nearest_repeat: sampler;
@group(0) @binding(16) var bilinear_repeat: sampler;
@group(0) @binding(17) var trilinear_repeat: sampler;
`;
}

function buildHelpers(passF32: boolean): string {
  const passSample = passF32
    ? `
  let res = float2(textureDimensions(pass_in));
  let st = uv * res - 0.5;
  let iuv = floor(st);
  let fuv = fract(st);
  let a = textureSampleLevel(pass_in, nearest, fract((iuv + float2(0.5,0.5)) / res), pass_index, lod);
  let b = textureSampleLevel(pass_in, nearest, fract((iuv + float2(1.5,0.5)) / res), pass_index, lod);
  let c = textureSampleLevel(pass_in, nearest, fract((iuv + float2(0.5,1.5)) / res), pass_index, lod);
  let d = textureSampleLevel(pass_in, nearest, fract((iuv + float2(1.5,1.5)) / res), pass_index, lod);
  return mix(mix(a, b, fuv.x), mix(c, d, fuv.x), fuv.y);`
    : `
  return textureSampleLevel(pass_in, bilinear, fract(uv), pass_index, lod);`;
  return `
fn keyDown(keycode: uint) -> bool {
  return ((_keyboard[keycode / 128u][(keycode % 128u) / 32u] >> (keycode % 32u)) & 1u) == 1u;
}
fn assert(index: int, success: bool) {}
fn passStore(pass_index: int, coord: int2, value: float4) {
  textureStore(pass_out, coord, pass_index, value);
}
fn passLoad(pass_index: int, coord: int2, lod: int) -> float4 {
  return textureLoad(pass_in, coord, pass_index, lod);
}
fn passSampleLevelBilinearRepeat(pass_index: int, uv: float2, lod: float) -> float4 {${passSample}
}
`;
}

function buildCustomStruct(names: string[]): string {
  const fields = names.map((n) => `  ${n}: float,`).join("\n");
  return `struct Custom {\n${fields}\n};\n`;
}

// ---------------------------------------------------------------------------
// Preprocessor — handles the compute.toys directives.
// ---------------------------------------------------------------------------

interface EntryInfo {
  name: string;
  wg: [number, number, number];
}

interface Preprocessed {
  body: string;
  extensions: string;
  entries: EntryInfo[];
  workgroupCounts: Record<string, [number, number, number]>;
  dispatchOnce: Record<string, boolean>;
  dispatchCount: Record<string, number>;
  storageCount: number;
}

function preprocess(shader: ComputeToysShader): Preprocessed {
  const macros: Record<string, string> = {};
  const workgroupCounts: Record<string, [number, number, number]> = {};
  const dispatchOnce: Record<string, boolean> = {};
  const dispatchCount: Record<string, number> = {};
  let storageCount = 0;
  let extensions = "";

  const lines = shader.code.split("\n");
  const kept: string[] = [];

  // First pass: collect #define values (needed to resolve directive args).
  for (const line of lines) {
    const t = line.trim();
    if (t.startsWith("#define ")) {
      const m = t.match(/^#define\s+(\w+)\s+(.+)$/);
      if (m) {
        macros[m[1]!] = m[2]!.trim();
      }
    }
  }
  const resolveToken = (s: string): string => macros[s] ?? s;
  const toInt = (s: string): number => parseInt(resolveToken(s), 10);

  for (const line of lines) {
    const t = line.trim();

    if (t.startsWith("enable") || t.startsWith("requires") || t.startsWith("diagnostic")) {
      extensions += line.replace(/\/\/.*$/, "") + "\n";
      continue;
    }
    if (t.startsWith("#define ")) {
      continue;
    }
    if (t.startsWith("#storage ")) {
      const m = t.match(/^#storage\s+(\w+)\s+(.+)$/);
      if (m && storageCount < 2) {
        kept.push(
          `@group(0) @binding(${storageCount}) var<storage,read_write> ${m[1]}: ${m[2]!.trim()};`,
        );
        storageCount++;
      }
      continue;
    }
    if (t.startsWith("#workgroup_count ")) {
      const m = t.match(/^#workgroup_count\s+(\w+)\s+(\S+)\s+(\S+)\s+(\S+)/);
      if (m) {
        workgroupCounts[m[1]!] = [toInt(m[2]!), toInt(m[3]!), toInt(m[4]!)];
      }
      continue;
    }
    if (t.startsWith("#dispatch_once ")) {
      const m = t.match(/^#dispatch_once\s+(\w+)/);
      if (m) {
        dispatchOnce[m[1]!] = true;
      }
      continue;
    }
    if (t.startsWith("#dispatch_count ")) {
      const m = t.match(/^#dispatch_count\s+(\w+)\s+(\S+)/);
      if (m) {
        dispatchCount[m[1]!] = toInt(m[2]!);
      }
      continue;
    }
    kept.push(line);
  }

  let body = kept.join("\n");

  // Substitute object-like macros (longest first to avoid prefix clashes).
  for (const name of Object.keys(macros).sort((a, b) => b.length - a.length)) {
    body = body.replace(new RegExp(`\\b${name}\\b`, "g"), macros[name]!);
  }

  // SCREEN_WIDTH/HEIGHT as runtime expressions (robust to resize without
  // recompiling). Const-context uses are not supported.
  body = body
    .replace(/\bSCREEN_WIDTH\b/g, "f32(textureDimensions(screen).x)")
    .replace(/\bSCREEN_HEIGHT\b/g, "f32(textureDimensions(screen).y)");

  // Discover compute entry points in source order.
  const entries: EntryInfo[] = [];
  const entryRe = /@compute[^@]*?@workgroup_size\(([^)]*)\)[^@]*?fn\s+(\w+)/g;
  let match: RegExpExecArray | null;
  while ((match = entryRe.exec(body)) !== null) {
    const sizes = match[1]!
      .split(",")
      .map((s) => parseInt(s.trim(), 10));
    entries.push({
      name: match[2]!,
      wg: [sizes[0] || 1, sizes[1] || 1, sizes[2] || 1],
    });
  }

  return {
    body,
    extensions,
    entries,
    workgroupCounts,
    dispatchOnce,
    dispatchCount,
    storageCount,
  };
}

// ---------------------------------------------------------------------------
// Pass factory
// ---------------------------------------------------------------------------

export function createComputeToysPass(
  ctx: HeroContext,
  shader: ComputeToysShader,
): Pass {
  const { device, format } = ctx;
  const passF32 = shader.passF32 ?? false;
  const passFormat: GPUTextureFormat = passF32 ? "rgba32float" : "rgba16float";
  const pre = preprocess(shader);

  const prelude =
    pre.extensions +
    buildAliases() +
    `struct Time { frame: u32, elapsed: f32, delta: f32 };\n` +
    `struct Mouse { pos: vec2<u32>, click: i32 };\n` +
    `struct DispatchInfo { id: u32 };\n` +
    buildCustomStruct(shader.customOrder) +
    buildBindingDecls(passF32) +
    buildHelpers(passF32);

  const code = `${prelude}\n${pre.body}`;

  device.pushErrorScope("validation");
  const module = device.createShaderModule({ code });
  device.popErrorScope().then((err) => {
    if (err) {
      // eslint-disable-next-line no-console
      console.error("[hero] compute.toys shader compile error:", err.message);
    }
  });
  module.getCompilationInfo?.().then((info) => {
    for (const msg of info.messages) {
      if (msg.type === "error") {
        // eslint-disable-next-line no-console
        console.error(`[hero] WGSL error (line ${msg.lineNum}): ${msg.message}`);
      }
    }
  });

  // --- Bind group layout (fixed) ---
  const uniform: GPUBufferBindingLayout = { type: "uniform" };
  const storage: GPUBufferBindingLayout = { type: "storage" };
  const tex2dArray: GPUTextureBindingLayout = {
    sampleType: passF32 ? "unfilterable-float" : "float",
    viewDimension: "2d-array",
  };
  const channelLayout: GPUTextureBindingLayout = {
    sampleType: "float",
    viewDimension: "2d",
  };
  const filtering: GPUSamplerBindingLayout = { type: "filtering" };
  const nonFiltering: GPUSamplerBindingLayout = { type: "non-filtering" };
  const V = GPUShaderStage.COMPUTE;

  const bindGroupLayout = device.createBindGroupLayout({
    entries: [
      { binding: 0, visibility: V, buffer: storage },
      { binding: 1, visibility: V, buffer: storage },
      { binding: 2, visibility: V, buffer: uniform },
      { binding: 3, visibility: V, buffer: uniform },
      { binding: 4, visibility: V, buffer: uniform },
      { binding: 5, visibility: V, buffer: uniform },
      {
        binding: 6,
        visibility: V,
        buffer: { type: "uniform", hasDynamicOffset: true },
      },
      {
        binding: 7,
        visibility: V,
        storageTexture: { access: "write-only", format: SCREEN_FORMAT },
      },
      { binding: 8, visibility: V, texture: tex2dArray },
      {
        binding: 9,
        visibility: V,
        storageTexture: {
          access: "write-only",
          format: passFormat,
          viewDimension: "2d-array",
        },
      },
      { binding: 10, visibility: V, texture: channelLayout },
      { binding: 11, visibility: V, texture: channelLayout },
      { binding: 12, visibility: V, sampler: nonFiltering },
      { binding: 13, visibility: V, sampler: filtering },
      { binding: 14, visibility: V, sampler: filtering },
      { binding: 15, visibility: V, sampler: nonFiltering },
      { binding: 16, visibility: V, sampler: filtering },
      { binding: 17, visibility: V, sampler: filtering },
    ],
  });

  const pipelineLayout = device.createPipelineLayout({
    bindGroupLayouts: [bindGroupLayout],
  });

  const pipelines = pre.entries.map((e) => ({
    info: e,
    once: pre.dispatchOnce[e.name] ?? false,
    count: pre.dispatchCount[e.name] ?? 1,
    wgCount: pre.workgroupCounts[e.name],
    pipeline: device.createComputePipeline({
      layout: pipelineLayout,
      compute: { module, entryPoint: e.name },
    }),
  }));

  // --- Resources ---
  const mkBuf = (size: number, usage: number) =>
    device.createBuffer({ size, usage });

  const storage1 = mkBuf(
    pre.storageCount >= 1 ? STORAGE_SIZE : 16,
    GPUBufferUsage.STORAGE,
  );
  const storage2 = mkBuf(
    pre.storageCount >= 2 ? STORAGE_SIZE : 16,
    GPUBufferUsage.STORAGE,
  );
  const timeBuf = mkBuf(16, GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST);
  const mouseBuf = mkBuf(16, GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST);
  const keysBuf = mkBuf(32, GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST);
  const customBuf = mkBuf(
    MAX_CUSTOM_PARAMS * 4,
    GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
  );
  const dispatchBuf = mkBuf(
    256 * OFFSET_ALIGNMENT,
    GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
  );

  // Custom uniforms are static; write once.
  const customData = new Float32Array(MAX_CUSTOM_PARAMS);
  shader.customOrder.forEach((name, i) => {
    customData[i] = shader.custom[name] ?? 0;
  });
  device.queue.writeBuffer(customBuf, 0, customData);

  // Blank 1x1 channel textures.
  const blank = (): GPUTexture =>
    device.createTexture({
      size: [1, 1],
      format: "rgba8unorm",
      usage: GPUTextureUsage.TEXTURE_BINDING,
    });
  const channel0 = blank();
  const channel1 = blank();

  // Samplers.
  const mkSampler = (linear: boolean, repeat: boolean) =>
    device.createSampler({
      magFilter: linear ? "linear" : "nearest",
      minFilter: linear ? "linear" : "nearest",
      mipmapFilter: linear ? "linear" : "nearest",
      ...(repeat
        ? { addressModeU: "repeat", addressModeV: "repeat", addressModeW: "repeat" }
        : {}),
    });
  const sNearest = mkSampler(false, false);
  const sBilinear = mkSampler(true, false);
  const sTrilinear = mkSampler(true, false);
  const sNearestR = mkSampler(false, true);
  const sBilinearR = mkSampler(true, true);
  const sTrilinearR = mkSampler(true, true);

  // Blit pipeline (screen -> swapchain, with upscaling).
  const blitModule = device.createShaderModule({ code: BLIT_SHADER });
  const blitLayout = device.createBindGroupLayout({
    entries: [
      { binding: 0, visibility: GPUShaderStage.FRAGMENT, texture: { sampleType: "float" } },
      { binding: 1, visibility: GPUShaderStage.FRAGMENT, sampler: { type: "filtering" } },
    ],
  });
  const blitPipeline = device.createRenderPipeline({
    layout: device.createPipelineLayout({ bindGroupLayouts: [blitLayout] }),
    vertex: { module: blitModule, entryPoint: "vs_main" },
    fragment: { module: blitModule, entryPoint: "fs_main", targets: [{ format }] },
    primitive: { topology: "triangle-list" },
  });
  const blitSampler = device.createSampler({ magFilter: "linear", minFilter: "linear" });

  // Resolution-dependent resources.
  const maxDim = shader.maxDimension ?? 1024;
  let width = 0;
  let height = 0;
  let screen: GPUTexture | null = null;
  let passRead: GPUTexture | null = null;
  let passWrite: GPUTexture | null = null;
  let bindGroup: GPUBindGroup | null = null;
  let blitBind: GPUBindGroup | null = null;
  let currentFrame = 0;

  const buildBindGroup = () => {
    if (!screen || !passRead || !passWrite) {
      return;
    }
    bindGroup = device.createBindGroup({
      layout: bindGroupLayout,
      entries: [
        { binding: 0, resource: { buffer: storage1 } },
        { binding: 1, resource: { buffer: storage2 } },
        { binding: 2, resource: { buffer: timeBuf } },
        { binding: 3, resource: { buffer: mouseBuf } },
        { binding: 4, resource: { buffer: keysBuf } },
        { binding: 5, resource: { buffer: customBuf } },
        { binding: 6, resource: { buffer: dispatchBuf, size: 4 } },
        { binding: 7, resource: screen.createView() },
        { binding: 8, resource: passRead.createView({ dimension: "2d-array" }) },
        { binding: 9, resource: passWrite.createView({ dimension: "2d-array" }) },
        { binding: 10, resource: channel0.createView() },
        { binding: 11, resource: channel1.createView() },
        { binding: 12, resource: sNearest },
        { binding: 13, resource: sBilinear },
        { binding: 14, resource: sTrilinear },
        { binding: 15, resource: sNearestR },
        { binding: 16, resource: sBilinearR },
        { binding: 17, resource: sTrilinearR },
      ],
    });
    blitBind = device.createBindGroup({
      layout: blitLayout,
      entries: [
        { binding: 0, resource: screen.createView() },
        { binding: 1, resource: blitSampler },
      ],
    });
  };

  const resize = (reqW: number, reqH: number) => {
    const scale = Math.min(1, maxDim / Math.max(reqW, reqH));
    const w = Math.max(1, Math.round(reqW * scale));
    const h = Math.max(1, Math.round(reqH * scale));
    if (w === width && h === height && screen) {
      return;
    }
    width = w;
    height = h;
    screen?.destroy();
    passRead?.destroy();
    passWrite?.destroy();
    screen = device.createTexture({
      size: [width, height],
      format: SCREEN_FORMAT,
      usage: GPUTextureUsage.STORAGE_BINDING | GPUTextureUsage.TEXTURE_BINDING,
    });
    passRead = device.createTexture({
      size: [width, height, PASS_LAYERS],
      format: passFormat,
      dimension: "2d",
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.TEXTURE_BINDING,
    });
    passWrite = device.createTexture({
      size: [width, height, PASS_LAYERS],
      format: passFormat,
      dimension: "2d",
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.STORAGE_BINDING,
    });
    buildBindGroup();
  };

  const orbitPeriod = shader.orbitPeriod ?? 0;

  return {
    update: (frame, timeSeconds) => {
      currentFrame = frame;
      const t = new ArrayBuffer(16);
      const tv = new DataView(t);
      tv.setUint32(0, frame, true);
      tv.setFloat32(4, timeSeconds, true);
      tv.setFloat32(8, 1 / 60, true);
      device.queue.writeBuffer(timeBuf, 0, t);

      const m = new ArrayBuffer(16);
      const mv = new DataView(m);
      const px = orbitPeriod > 0 ? Math.round(((timeSeconds / orbitPeriod) % 1) * width) : 0;
      mv.setUint32(0, px >>> 0, true);
      mv.setUint32(4, 0, true);
      mv.setInt32(8, 0, true);
      device.queue.writeBuffer(mouseBuf, 0, m);
    },
    encode: (encoder, targetView) => {
      if (!bindGroup || !blitBind || !passRead || !passWrite) {
        return;
      }
      let counter = 0;
      for (const p of pipelines) {
        if (p.once && currentFrame !== 0) {
          continue;
        }
        for (let i = 0; i < p.count; i++) {
          device.queue.writeBuffer(
            dispatchBuf,
            counter * OFFSET_ALIGNMENT,
            new Uint32Array([i]),
          );
          const [gx, gy, gz] = p.wgCount ?? [
            Math.ceil(width / p.info.wg[0]),
            Math.ceil(height / p.info.wg[1]),
            1,
          ];
          const cpass = encoder.beginComputePass();
          cpass.setPipeline(p.pipeline);
          cpass.setBindGroup(0, bindGroup, [counter * OFFSET_ALIGNMENT]);
          cpass.dispatchWorkgroups(Math.max(1, gx), Math.max(1, gy), Math.max(1, gz));
          cpass.end();

          // Ping-pong: make pass_out visible as pass_in for the next pass.
          encoder.copyTextureToTexture(
            { texture: passWrite },
            { texture: passRead },
            { width, height, depthOrArrayLayers: PASS_LAYERS },
          );
          counter++;
        }
      }

      const rpass = encoder.beginRenderPass({
        colorAttachments: [
          { view: targetView, clearValue: [0, 0, 0, 1], loadOp: "clear", storeOp: "store" },
        ],
      });
      rpass.setPipeline(blitPipeline);
      rpass.setBindGroup(0, blitBind);
      rpass.draw(3);
      rpass.end();
    },
    resize,
    destroy: () => {
      screen?.destroy();
      passRead?.destroy();
      passWrite?.destroy();
      storage1.destroy();
      storage2.destroy();
      timeBuf.destroy();
      mouseBuf.destroy();
      keysBuf.destroy();
      customBuf.destroy();
      dispatchBuf.destroy();
      channel0.destroy();
      channel1.destroy();
    },
  };
}
