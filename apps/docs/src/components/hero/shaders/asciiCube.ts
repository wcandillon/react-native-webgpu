// First-party hero shader: an ASCII-dithered SDF that slowly morphs between a
// cube and a sphere. Migrated from the original standalone HeroShader. Uses the
// standard uniforms plus a font-atlas texture/sampler in @group(1).

import type { HeroContext, ShaderEntry, ShaderResources } from "../types";

const CHARSET = " .'`^\",:;!|#@█";
const CHAR_W = 8;
const CHAR_H = 12;
const CHAR_SIZE_BASE = 10; // logical px per cell, scaled by dpr in the shader

const CODE = /* wgsl */ `
const CHAR_COUNT: f32 = ${CHARSET.length}.0;
const CHAR_SIZE_BASE: f32 = ${CHAR_SIZE_BASE}.0;

@group(1) @binding(0) var fontAtlas: texture_2d<f32>;
@group(1) @binding(1) var fontSampler: sampler;

fn rotX(p: vec3f, a: f32) -> vec3f {
  let s = sin(a); let c = cos(a);
  return vec3f(p.x, c * p.y - s * p.z, s * p.y + c * p.z);
}
fn rotY(p: vec3f, a: f32) -> vec3f {
  let s = sin(a); let c = cos(a);
  return vec3f(c * p.x + s * p.z, p.y, -s * p.x + c * p.z);
}
fn rotZ(p: vec3f, a: f32) -> vec3f {
  let s = sin(a); let c = cos(a);
  return vec3f(c * p.x - s * p.y, s * p.x + c * p.y, p.z);
}

fn sdfBox(p: vec3f, halfExtent: vec3f) -> f32 {
  let q = abs(p) - halfExtent;
  return length(max(q, vec3f(0.0))) + min(max(q.x, max(q.y, q.z)), 0.0);
}
fn sdfSphere(p: vec3f, radius: f32) -> f32 { return length(p) - radius; }
fn sdfScene(p: vec3f, morph: f32) -> f32 {
  return mix(sdfBox(p, vec3f(1.0)), sdfSphere(p, 1.0), morph);
}
fn sdfNormal(p: vec3f, morph: f32) -> vec3f {
  let eps = 0.0015;
  let d = sdfScene(p, morph);
  let nx = sdfScene(p + vec3f(eps, 0.0, 0.0), morph) - d;
  let ny = sdfScene(p + vec3f(0.0, eps, 0.0), morph) - d;
  let nz = sdfScene(p + vec3f(0.0, 0.0, eps), morph) - d;
  return normalize(vec3f(nx, ny, nz));
}
fn traceScene(ro: vec3f, rd: vec3f, morph: f32) -> f32 {
  var t = 0.0;
  for (var i = 0; i < 80; i++) {
    let p = ro + rd * t;
    let d = sdfScene(p, morph);
    if (d < 0.0008) { return t; }
    t += max(d * 0.9, 0.001);
    if (t > 24.0) { break; }
  }
  return -1.0;
}

fn bayer8(p: vec2f) -> f32 {
  var mx = u32(p.x) & 7u;
  var my = u32(p.y) & 7u;
  var acc = 0u;
  for (var i = 0u; i < 3u; i++) {
    let ox = mx & 1u; let oy = my & 1u;
    acc = acc * 4u + oy * 2u + ox;
    mx = mx >> 1u; my = my >> 1u;
  }
  return f32(acc) / 64.0;
}
fn imageDither(col: vec3f, px: vec2f) -> vec3f {
  let levels = 22.0;
  let strength = 1.6;
  let dR = bayer8(px) - 0.5;
  let dG = bayer8(px + vec2f(3.0, 1.0)) - 0.5;
  let dB = bayer8(px + vec2f(1.0, 5.0)) - 0.5;
  return vec3f(
    (floor(col.r * levels + dR * strength) + 0.5) / levels,
    (floor(col.g * levels + dG * strength) + 0.5) / levels,
    (floor(col.b * levels + dB * strength) + 0.5) / levels,
  );
}
fn themeBg(theme: f32) -> vec3f {
  return mix(vec3f(0.07, 0.07, 0.09), vec3f(0.90, 0.905, 0.925), theme);
}
fn faceTone(n: vec3f) -> f32 {
  let a = abs(n);
  if (a.x > a.y && a.x > a.z) { return mix(0.48, 0.52, step(0.0, n.x)); }
  if (a.y > a.z) { return mix(0.46, 0.50, step(0.0, n.y)); }
  return mix(0.49, 0.53, step(0.0, n.z));
}
fn surfaceTone(n: vec3f, shape: f32) -> f32 {
  let box = faceTone(n);
  let sphere = 0.50 + n.y * 0.035 + n.z * 0.015;
  return mix(box, sphere, shape);
}
fn cubeSample(pixel: vec2f, shape: f32) -> vec4f {
  let aspect = u.resolution.x / max(u.resolution.y, 1.0);
  var uv = pixel / u.resolution * 2.0 - 1.0;
  uv.x *= aspect;

  let ro = vec3f(0.0, 0.0, -6.8);
  let rd = normalize(vec3f(uv * 0.46, 1.6));

  let yaw = u.time * 0.38;
  let pitch = u.time * 0.14;
  let roll = u.time * 0.091;

  var lro = rotY(ro, -yaw);
  var lrd = rotY(rd, -yaw);
  lro = rotX(lro, -pitch); lrd = rotX(lrd, -pitch);
  lro = rotZ(lro, -roll);  lrd = rotZ(lrd, -roll);

  let hitT = traceScene(lro, lrd, shape);
  if (hitT < 0.0) { return vec4f(0.0); }

  let pt = lro + lrd * hitT;
  let n = sdfNormal(pt, shape);
  let light = normalize(vec3f(0.35, 0.55, 0.75));
  let diff = clamp(dot(n, light), 0.0, 1.0);
  let rim = pow(1.0 - clamp(dot(n, -lrd), 0.0, 1.0), 2.0) * 0.18;

  let tone = surfaceTone(n, shape);
  let shade = clamp(diff * 0.62 + rim + 0.26 + (tone - 0.5) * 0.10, 0.0, 1.0);
  return vec4f(tone, tone, tone, shade);
}

@fragment
fn fs_main(@builtin(position) pos: vec4f) -> @location(0) vec4f {
  let charSize = max(u.dpr, 1.0) * CHAR_SIZE_BASE;
  // Slow auto-morph between cube (0) and sphere (1), mostly resting on the cube.
  let shape = smoothstep(0.55, 0.95, 0.5 + 0.5 * sin(u.time * 0.16));

  let cell = floor(pos.xy / charSize);
  let cellCenter = (cell + 0.5) * charSize;
  let sample = cubeSample(cellCenter, shape);
  let shade = sample.a;
  let face = sample.r;

  let idx = u32(clamp(shade * (CHAR_COUNT - 1.0), 0.0, CHAR_COUNT - 1.0));
  let local = fract(pos.xy / charSize);
  let atlasX = (f32(idx) + local.x) / CHAR_COUNT;
  let atlasY = local.y;
  let glyph = textureSample(fontAtlas, fontSampler, vec2f(atlasX, atlasY)).r;

  let bg = themeBg(u.theme);
  let mark = glyph * (0.34 + shade * 0.38);
  let ink = vec3f(0.54, 0.54, 0.56) * (0.94 + face * 0.06);
  let col = mix(
    bg + ink * mark * 0.19,
    bg - ink * mark * 0.25,
    u.theme,
  );

  if (shade < 0.02 || (glyph < 0.04 && shade < 0.10)) {
    return vec4f(imageDither(bg, floor(pos.xy)), 1.0);
  }
  return vec4f(imageDither(col, floor(pos.xy)), 1.0);
}
`;

function createFontAtlas(): {
  data: Uint8Array;
  width: number;
  height: number;
} {
  const width = CHAR_W * CHARSET.length;
  const height = CHAR_H;
  const canvas = document.createElement("canvas");
  canvas.width = width;
  canvas.height = height;

  const ctx = canvas.getContext("2d");
  if (!ctx) {
    return { data: new Uint8Array(width * height), width, height };
  }

  ctx.fillStyle = "#000000";
  ctx.fillRect(0, 0, width, height);
  ctx.fillStyle = "#ffffff";
  ctx.font = `bold ${CHAR_H - 1}px ui-monospace, SFMono-Regular, Menlo, monospace`;
  ctx.textBaseline = "middle";

  for (let i = 0; i < CHARSET.length; i++) {
    ctx.fillText(CHARSET[i]!, i * CHAR_W + 1, height / 2 + 1);
  }

  const { data } = ctx.getImageData(0, 0, width, height);
  const luminance = new Uint8Array(width * height);
  for (let i = 0; i < width * height; i++) {
    const r = data[i * 4]!;
    const g = data[i * 4 + 1]!;
    const b = data[i * 4 + 2]!;
    luminance[i] = Math.round(r * 0.299 + g * 0.587 + b * 0.114);
  }
  return { data: luminance, width, height };
}

function fontResources({ device }: HeroContext): ShaderResources {
  const atlas = createFontAtlas();
  const texture = device.createTexture({
    size: [atlas.width, atlas.height],
    format: "r8unorm",
    usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST,
  });
  device.queue.writeTexture(
    { texture },
    new Uint8Array(atlas.data),
    { bytesPerRow: atlas.width },
    [atlas.width, atlas.height],
  );

  const sampler = device.createSampler({
    magFilter: "nearest",
    minFilter: "nearest",
  });

  const bindGroupLayout = device.createBindGroupLayout({
    entries: [
      {
        binding: 0,
        visibility: GPUShaderStage.FRAGMENT,
        texture: { sampleType: "float" },
      },
      {
        binding: 1,
        visibility: GPUShaderStage.FRAGMENT,
        sampler: { type: "filtering" },
      },
    ],
  });

  const bindGroup = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: texture.createView() },
      { binding: 1, resource: sampler },
    ],
  });

  return {
    bindGroupLayout,
    bindGroup,
    destroy: () => texture.destroy(),
  };
}

export const asciiCube: ShaderEntry = {
  id: "ascii-cube",
  title: "ASCII SDF",
  author: "William Candillon",
  sourceUrl: "https://github.com/wcandillon/react-native-webgpu",
  license: "MIT",
  shader: {
    kind: "fragment",
    code: CODE,
    resources: fontResources,
  },
};
