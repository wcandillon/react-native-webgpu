// Builds a render/compute "pass" for a single shader entry. The HeroShader
// component owns the device, swapchain, uniform buffer, and animation loop, and
// swaps the active pass when it auto-cycles.

import { createComputeToysPass } from "./computetoys";
import {
  BLIT_SHADER,
  COMPUTE_SCREEN_BINDING,
  FADE_OVERLAY_SHADER,
  FULLSCREEN_VERTEX,
  SCREEN_FORMAT,
  UNIFORM_STRUCT,
} from "./prelude";
import type {
  ComputeShader,
  ComputeToysShader,
  FragmentShader,
  HeroContext,
  ShaderResources,
} from "./types";

export interface Pass {
  encode: (encoder: GPUCommandEncoder, view: GPUTextureView) => void;
  resize: (width: number, height: number) => void;
  update: (frame: number, timeSeconds: number) => void;
  // Pointer state for interactive shaders. nx/ny are normalized [0,1] over the
  // canvas; down is whether the pointer is pressed. Optional: only the
  // compute.toys runtime consumes it.
  setPointer?: (nx: number, ny: number, down: boolean) => void;
  destroy: () => void;
}

// Fades the shader in from the hero background color: a fullscreen quad
// alpha-blended over the finished frame. The caller drives `alpha` from 1 to 0
// (over the first second) and stops encoding it once it reaches 0.
export interface FadeOverlay {
  encode: (encoder: GPUCommandEncoder, view: GPUTextureView, alpha: number) => void;
  destroy: () => void;
}

export function createFadeOverlay(
  ctx: HeroContext,
  color: [number, number, number],
): FadeOverlay {
  const { device, format } = ctx;
  const module = device.createShaderModule({ code: FADE_OVERLAY_SHADER });
  const layout = device.createBindGroupLayout({
    entries: [
      {
        binding: 0,
        visibility: GPUShaderStage.FRAGMENT,
        buffer: { type: "uniform" },
      },
    ],
  });
  const pipeline = device.createRenderPipeline({
    layout: device.createPipelineLayout({ bindGroupLayouts: [layout] }),
    vertex: { module, entryPoint: "vs_main" },
    fragment: {
      module,
      entryPoint: "fs_main",
      targets: [
        {
          format,
          blend: {
            color: { srcFactor: "src-alpha", dstFactor: "one-minus-src-alpha" },
            alpha: { srcFactor: "one", dstFactor: "one-minus-src-alpha" },
          },
        },
      ],
    },
    primitive: { topology: "triangle-list" },
  });
  const buffer = device.createBuffer({
    size: 16,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
  });
  const bind = device.createBindGroup({
    layout,
    entries: [{ binding: 0, resource: { buffer } }],
  });
  const data = new Float32Array([...color, 1]);

  return {
    encode: (encoder, view, alpha) => {
      data[3] = alpha;
      device.queue.writeBuffer(buffer, 0, data);
      const pass = encoder.beginRenderPass({
        colorAttachments: [{ view, loadOp: "load", storeOp: "store" }],
      });
      pass.setPipeline(pipeline);
      pass.setBindGroup(0, bind);
      pass.draw(3);
      pass.end();
    },
    destroy: () => buffer.destroy(),
  };
}

function createFragmentPass(
  ctx: HeroContext,
  shader: FragmentShader,
  uniformBuffer: GPUBuffer,
): Pass {
  const { device, format } = ctx;
  const res: ShaderResources | undefined = shader.resources?.(ctx);

  const module = device.createShaderModule({
    code: UNIFORM_STRUCT + FULLSCREEN_VERTEX + shader.code,
  });

  const stdLayout = device.createBindGroupLayout({
    entries: [
      {
        binding: 0,
        visibility: GPUShaderStage.FRAGMENT,
        buffer: { type: "uniform" },
      },
    ],
  });

  const layouts = res ? [stdLayout, res.bindGroupLayout] : [stdLayout];

  const pipeline = device.createRenderPipeline({
    layout: device.createPipelineLayout({ bindGroupLayouts: layouts }),
    vertex: { module, entryPoint: "vs_main" },
    fragment: { module, entryPoint: "fs_main", targets: [{ format }] },
    primitive: { topology: "triangle-list" },
  });

  const stdBind = device.createBindGroup({
    layout: stdLayout,
    entries: [{ binding: 0, resource: { buffer: uniformBuffer } }],
  });

  return {
    encode: (encoder, view) => {
      const pass = encoder.beginRenderPass({
        colorAttachments: [
          {
            view,
            clearValue: [0, 0, 0, 1],
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      });
      pass.setPipeline(pipeline);
      pass.setBindGroup(0, stdBind);
      if (res) {
        pass.setBindGroup(1, res.bindGroup);
      }
      pass.draw(3);
      pass.end();
    },
    resize: () => {},
    update: (frame, time) => res?.update?.(frame, time),
    destroy: () => res?.destroy?.(),
  };
}

function createComputePass(
  ctx: HeroContext,
  shader: ComputeShader,
  uniformBuffer: GPUBuffer,
): Pass {
  const { device, format } = ctx;
  const res: ShaderResources | undefined = shader.resources?.(ctx);
  const [wgX, wgY] = shader.workgroupSize;

  const module = device.createShaderModule({
    code: UNIFORM_STRUCT + COMPUTE_SCREEN_BINDING + shader.code,
  });

  const stdLayout = device.createBindGroupLayout({
    entries: [
      {
        binding: 0,
        visibility: GPUShaderStage.COMPUTE,
        buffer: { type: "uniform" },
      },
      {
        binding: 1,
        visibility: GPUShaderStage.COMPUTE,
        storageTexture: { access: "write-only", format: SCREEN_FORMAT },
      },
    ],
  });

  const layouts = res ? [stdLayout, res.bindGroupLayout] : [stdLayout];

  const pipeline = device.createComputePipeline({
    layout: device.createPipelineLayout({ bindGroupLayouts: layouts }),
    compute: { module, entryPoint: "main" },
  });

  // Blit: sample the compute output onto the swapchain.
  const blitModule = device.createShaderModule({ code: BLIT_SHADER });
  const blitLayout = device.createBindGroupLayout({
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
  const blitPipeline = device.createRenderPipeline({
    layout: device.createPipelineLayout({ bindGroupLayouts: [blitLayout] }),
    vertex: { module: blitModule, entryPoint: "vs_main" },
    fragment: { module: blitModule, entryPoint: "fs_main", targets: [{ format }] },
    primitive: { topology: "triangle-list" },
  });
  const blitSampler = device.createSampler({
    magFilter: "linear",
    minFilter: "linear",
  });

  let screen: GPUTexture | null = null;
  let stdBind: GPUBindGroup | null = null;
  let blitBind: GPUBindGroup | null = null;
  let width = 0;
  let height = 0;

  const resize = (w: number, h: number) => {
    if (w === width && h === height && screen) {
      return;
    }
    width = Math.max(1, w);
    height = Math.max(1, h);
    screen?.destroy();
    screen = device.createTexture({
      size: [width, height],
      format: SCREEN_FORMAT,
      usage: GPUTextureUsage.STORAGE_BINDING | GPUTextureUsage.TEXTURE_BINDING,
    });
    const view = screen.createView();
    stdBind = device.createBindGroup({
      layout: stdLayout,
      entries: [
        { binding: 0, resource: { buffer: uniformBuffer } },
        { binding: 1, resource: view },
      ],
    });
    blitBind = device.createBindGroup({
      layout: blitLayout,
      entries: [
        { binding: 0, resource: view },
        { binding: 1, resource: blitSampler },
      ],
    });
  };

  return {
    encode: (encoder, targetView) => {
      if (!stdBind || !blitBind) {
        return;
      }
      const cpass = encoder.beginComputePass();
      cpass.setPipeline(pipeline);
      cpass.setBindGroup(0, stdBind);
      if (res) {
        cpass.setBindGroup(1, res.bindGroup);
      }
      cpass.dispatchWorkgroups(
        Math.ceil(width / wgX),
        Math.ceil(height / wgY),
      );
      cpass.end();

      const rpass = encoder.beginRenderPass({
        colorAttachments: [
          {
            view: targetView,
            clearValue: [0, 0, 0, 1],
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      });
      rpass.setPipeline(blitPipeline);
      rpass.setBindGroup(0, blitBind);
      rpass.draw(3);
      rpass.end();
    },
    resize,
    update: (frame, time) => res?.update?.(frame, time),
    destroy: () => {
      screen?.destroy();
      res?.destroy?.();
    },
  };
}

export function createPass(
  ctx: HeroContext,
  shader: FragmentShader | ComputeShader | ComputeToysShader,
  uniformBuffer: GPUBuffer,
  opts: { isMobile?: boolean } = {},
): Pass {
  if (shader.kind === "computetoys") {
    // Imported lazily to avoid a cycle (computetoys.ts imports Pass from here).
    return createComputeToysPass(ctx, shader, opts);
  }
  return shader.kind === "compute"
    ? createComputePass(ctx, shader, uniformBuffer)
    : createFragmentPass(ctx, shader, uniformBuffer);
}
