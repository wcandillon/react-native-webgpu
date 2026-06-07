"use client";

import { useEffect, useRef } from "react";
import { useTheme } from "next-themes";

const CHARSET = " .'`^\",:;!|#@█";
const CHAR_W = 8;
const CHAR_H = 12;
const CHAR_SIZE = 10;

const VERTEX_SHADER = /* wgsl */ `
@vertex
fn vs_main(@builtin(vertex_index) vi: u32) -> @builtin(position) vec4f {
  var pos = array(
    vec2f(-1.0, -1.0),
    vec2f(3.0, -1.0),
    vec2f(-1.0, 3.0),
  );
  return vec4f(pos[vi], 0.0, 1.0);
}
`;

const FRAGMENT_SHADER = /* wgsl */ `
struct Uniforms {
  resolution: vec2f,
  time: f32,
  charSize: f32,
  charCount: f32,
  theme: f32,
  shape: f32,
}

@group(0) @binding(0) var<uniform> u: Uniforms;
@group(0) @binding(1) var fontAtlas: texture_2d<f32>;
@group(0) @binding(2) var fontSampler: sampler;

fn rotX(p: vec3f, a: f32) -> vec3f {
  let s = sin(a);
  let c = cos(a);
  return vec3f(p.x, c * p.y - s * p.z, s * p.y + c * p.z);
}

fn rotY(p: vec3f, a: f32) -> vec3f {
  let s = sin(a);
  let c = cos(a);
  return vec3f(c * p.x + s * p.z, p.y, -s * p.x + c * p.z);
}

fn sdfBox(p: vec3f, halfExtent: vec3f) -> f32 {
  let q = abs(p) - halfExtent;
  return length(max(q, vec3f(0.0))) + min(max(q.x, max(q.y, q.z)), 0.0);
}

fn sdfSphere(p: vec3f, radius: f32) -> f32 {
  return length(p) - radius;
}

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
    if (d < 0.0008) {
      return t;
    }
    t += max(d * 0.9, 0.001);
    if (t > 24.0) {
      break;
    }
  }
  return -1.0;
}

fn bayer8(p: vec2f) -> f32 {
  var mx = u32(p.x) & 7u;
  var my = u32(p.y) & 7u;
  var acc = 0u;
  for (var i = 0u; i < 3u; i++) {
    let ox = mx & 1u;
    let oy = my & 1u;
    acc = acc * 4u + oy * 2u + ox;
    mx = mx >> 1u;
    my = my >> 1u;
  }
  return f32(acc) / 64.0;
}

fn rotZ(p: vec3f, a: f32) -> vec3f {
  let s = sin(a);
  let c = cos(a);
  return vec3f(c * p.x - s * p.y, s * p.x + c * p.y, p.z);
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
  return mix(vec3f(0.018, 0.018, 0.03), vec3f(0.965, 0.968, 0.982), theme);
}

fn faceTone(n: vec3f) -> f32 {
  let a = abs(n);
  if (a.x > a.y && a.x > a.z) {
    return mix(0.48, 0.52, step(0.0, n.x));
  }
  if (a.y > a.z) {
    return mix(0.46, 0.50, step(0.0, n.y));
  }
  return mix(0.49, 0.53, step(0.0, n.z));
}

fn surfaceTone(n: vec3f, shape: f32) -> f32 {
  let box = faceTone(n);
  let sphere = 0.50 + n.y * 0.035 + n.z * 0.015;
  return mix(box, sphere, shape);
}

fn cubeSample(pixel: vec2f) -> vec4f {
  let aspect = u.resolution.x / max(u.resolution.y, 1.0);
  var uv = pixel / u.resolution * 2.0 - 1.0;
  uv.x *= aspect;

  let ro = vec3f(0.0, 0.0, -3.4);
  let rd = normalize(vec3f(uv * 0.92, 1.6));

  let yaw = u.time * 0.38;
  let pitch = u.time * 0.14;
  let roll = u.time * 0.091;

  var lro = rotY(ro, -yaw);
  var lrd = rotY(rd, -yaw);
  lro = rotX(lro, -pitch);
  lrd = rotX(lrd, -pitch);
  lro = rotZ(lro, -roll);
  lrd = rotZ(lrd, -roll);

  let hitT = traceScene(lro, lrd, u.shape);
  if (hitT < 0.0) {
    return vec4f(0.0, 0.0, 0.0, 0.0);
  }

  let pt = lro + lrd * hitT;
  let n = sdfNormal(pt, u.shape);
  let light = normalize(vec3f(0.35, 0.55, 0.75));
  let diff = clamp(dot(n, light), 0.0, 1.0);
  let rim = pow(1.0 - clamp(dot(n, -lrd), 0.0, 1.0), 2.0) * 0.18;

  let tone = surfaceTone(n, u.shape);
  let shade = clamp(diff * 0.62 + rim + 0.26 + (tone - 0.5) * 0.10, 0.0, 1.0);

  return vec4f(tone, tone, tone, shade);
}

@fragment
fn fs_main(@builtin(position) pos: vec4f) -> @location(0) vec4f {
  let cell = floor(pos.xy / u.charSize);
  let cellCenter = (cell + 0.5) * u.charSize;
  let sample = cubeSample(cellCenter);
  let shade = sample.a;
  let face = sample.r;

  let idx = u32(clamp(shade * (u.charCount - 1.0), 0.0, u.charCount - 1.0));

  let local = fract(pos.xy / u.charSize);
  let atlasX = (f32(idx) + local.x) / u.charCount;
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

function createFontAtlas(): Uint8Array {
  const width = CHAR_W * CHARSET.length;
  const height = CHAR_H;
  const canvas = document.createElement("canvas");
  canvas.width = width;
  canvas.height = height;

  const ctx = canvas.getContext("2d");
  if (!ctx) {
    return new Uint8Array(width * height);
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

  return luminance;
}

interface HeroShaderProps {
  className?: string;
}

const CLICKS_TO_MORPH = 5;
const CLICK_RESET_MS = 2500;
const MORPH_BACK_DEBOUNCE_MS = 700;

export function HeroShader({ className }: HeroShaderProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const { resolvedTheme } = useTheme();
  const themeTargetRef = useRef(0);
  const themeRef = useRef(0);
  const shapeTargetRef = useRef(0);
  const shapeRef = useRef(0);
  const clickCountRef = useRef(0);
  const lastClickAtRef = useRef(0);
  const sphereLockedAtRef = useRef(0);
  const lastTimeRef = useRef(0);

  useEffect(() => {
    themeTargetRef.current = resolvedTheme === "light" ? 1 : 0;
  }, [resolvedTheme]);

  const onCanvasClick = () => {
    const now = performance.now();

    if (shapeTargetRef.current >= 1) {
      if (now - sphereLockedAtRef.current < MORPH_BACK_DEBOUNCE_MS) {
        return;
      }
      shapeTargetRef.current = 0;
      clickCountRef.current = 0;
      return;
    }

    if (now - lastClickAtRef.current > CLICK_RESET_MS) {
      clickCountRef.current = 0;
    }
    lastClickAtRef.current = now;
    clickCountRef.current += 1;

    if (clickCountRef.current >= CLICKS_TO_MORPH) {
      shapeTargetRef.current = 1;
      sphereLockedAtRef.current = now;
      clickCountRef.current = 0;
    }
  };

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas || !navigator.gpu) {
      return;
    }

    let animationId = 0;
    let disposed = false;
    let device: GPUDevice | null = null;
    let uniformBuffer: GPUBuffer | null = null;
    let fontTexture: GPUTexture | null = null;
    let clearColor: GPUColor = [0.018, 0.018, 0.03, 1];

    (async () => {
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter || disposed) {
        return;
      }

      device = await adapter.requestDevice();
      if (disposed) {
        device.destroy();
        return;
      }

      const context = canvas.getContext("webgpu");
      if (!context) {
        return;
      }

      const format = navigator.gpu.getPreferredCanvasFormat();
      const atlasData = createFontAtlas();
      const atlasWidth = CHAR_W * CHARSET.length;
      const atlasHeight = CHAR_H;

      fontTexture = device.createTexture({
        size: [atlasWidth, atlasHeight],
        format: "r8unorm",
        usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST,
      });

      device.queue.writeTexture(
        { texture: fontTexture },
        new Uint8Array(atlasData),
        { bytesPerRow: atlasWidth },
        [atlasWidth, atlasHeight],
      );

      const sampler = device.createSampler({
        magFilter: "nearest",
        minFilter: "nearest",
      });

      const renderModule = device.createShaderModule({ code: FRAGMENT_SHADER });

      const bindGroupLayout = device.createBindGroupLayout({
        entries: [
          {
            binding: 0,
            visibility: GPUShaderStage.FRAGMENT,
            buffer: { type: "uniform" },
          },
          {
            binding: 1,
            visibility: GPUShaderStage.FRAGMENT,
            texture: { sampleType: "float" },
          },
          {
            binding: 2,
            visibility: GPUShaderStage.FRAGMENT,
            sampler: { type: "filtering" },
          },
        ],
      });

      const renderPipeline = device.createRenderPipeline({
        layout: device.createPipelineLayout({
          bindGroupLayouts: [bindGroupLayout],
        }),
        vertex: {
          module: device.createShaderModule({ code: VERTEX_SHADER }),
          entryPoint: "vs_main",
        },
        fragment: {
          module: renderModule,
          entryPoint: "fs_main",
          targets: [{ format }],
        },
        primitive: { topology: "triangle-list" },
      });

      uniformBuffer = device.createBuffer({
        size: 32,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
      });

      const bindGroup = device.createBindGroup({
        layout: bindGroupLayout,
        entries: [
          { binding: 0, resource: { buffer: uniformBuffer } },
          { binding: 1, resource: fontTexture.createView() },
          { binding: 2, resource: sampler },
        ],
      });

      const resize = () => {
        if (!device) return;

        const dpr = window.devicePixelRatio || 1;
        const { width, height } = canvas.getBoundingClientRect();
        canvas.width = Math.max(1, Math.floor(width * dpr));
        canvas.height = Math.max(1, Math.floor(height * dpr));
        context.configure({ device, format, alphaMode: "opaque" });
      };

      resize();
      window.addEventListener("resize", resize);
      lastTimeRef.current = performance.now();

      const render = (now: number) => {
        if (disposed || !device || !uniformBuffer) {
          return;
        }

        resize();

        const dt = Math.min(0.032, (now - lastTimeRef.current) / 1000);
        lastTimeRef.current = now;
        const time = now / 1000;
        const dpr = window.devicePixelRatio || 1;

        themeRef.current +=
          (themeTargetRef.current - themeRef.current) * (1 - Math.exp(-dt * 10));
        const theme = themeRef.current;

        shapeRef.current +=
          (shapeTargetRef.current - shapeRef.current) * (1 - Math.exp(-dt * 1.8));
        const shape = shapeRef.current;

        clearColor = theme > 0.5
          ? [0.965, 0.968, 0.982, 1]
          : [0.018, 0.018, 0.03, 1];

        const uniformData = new ArrayBuffer(32);
        new Float32Array(uniformData, 0, 2).set([
          canvas.width,
          canvas.height,
        ]);
        new Float32Array(uniformData, 8, 5).set([
          time,
          CHAR_SIZE * dpr,
          CHARSET.length,
          theme,
          shape,
        ]);
        device.queue.writeBuffer(uniformBuffer, 0, uniformData);

        const encoder = device.createCommandEncoder();
        const pass = encoder.beginRenderPass({
          colorAttachments: [
            {
              view: context.getCurrentTexture().createView(),
              clearValue: clearColor,
              loadOp: "clear",
              storeOp: "store",
            },
          ],
        });
        pass.setPipeline(renderPipeline);
        pass.setBindGroup(0, bindGroup);
        pass.draw(3);
        pass.end();

        device.queue.submit([encoder.finish()]);
        animationId = requestAnimationFrame(render);
      };

      animationId = requestAnimationFrame(render);

      return () => {
        window.removeEventListener("resize", resize);
      };
    })();

    return () => {
      disposed = true;
      cancelAnimationFrame(animationId);
      uniformBuffer?.destroy();
      fontTexture?.destroy();
      device?.destroy();
    };
  }, []);

  const isLight = resolvedTheme === "light";

  return (
    <canvas
      ref={canvasRef}
      className={className}
      onClick={onCanvasClick}
      style={{ background: isLight ? "#f6f6fa" : "#050508", cursor: "default" }}
    />
  );
}
