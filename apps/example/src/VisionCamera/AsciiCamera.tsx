import { fetchAsset } from "../components/useAssets";

import { useWebGPUCamera } from "./useWebGPUCamera";

// Real-time ASCII filter: each output cell samples the camera at its center,
// the luminance picks a glyph from a pre-baked atlas (' .:-=+*#%@', 10
// glyphs, dark to dense), and the glyph mask is multiplied by the cell's
// color so the result looks like a colored terminal printout of the scene.

const ATLAS_GLYPH_COUNT = 10;
// Each output cell is CELL_PX_W x CELL_PX_H physical pixels. The atlas's
// per-glyph aspect ratio is 1:2 (32x64), so cells are 1:2 too. 24x48 hits a
// nice density on most phone screens at @3x.
const CELL_PX_W = 24;
const CELL_PX_H = 48;

const SHADER = /* wgsl */ `
struct VsOut {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};

struct Uniforms {
  // x, y: cover-fit camera UV scale around (0.5, 0.5).
  // z, w: canvas resolution in pixels.
  scaleAndResolution: vec4f,
  // x, y: cell size in pixels.
  // z:    number of glyphs in the atlas.
  // w:    unused, padding.
  cellAndGlyphs: vec4f,
};

@group(0) @binding(0) var srcTex: texture_external;
@group(0) @binding(1) var srcSampler: sampler;
@group(0) @binding(2) var atlasTex: texture_2d<f32>;
@group(0) @binding(3) var atlasSampler: sampler;
@group(0) @binding(4) var<uniform> u: Uniforms;

@vertex
fn vs_main(@builtin(vertex_index) vid: u32) -> VsOut {
  var positions = array<vec2f, 3>(
    vec2f(-1.0, -3.0),
    vec2f(-1.0,  1.0),
    vec2f( 3.0,  1.0),
  );
  var uvs = array<vec2f, 3>(
    vec2f(0.0, 2.0),
    vec2f(0.0, 0.0),
    vec2f(2.0, 0.0),
  );
  var out: VsOut;
  out.position = vec4f(positions[vid], 0.0, 1.0);
  out.uv = uvs[vid];
  return out;
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4f {
  let uvScale = u.scaleAndResolution.xy;
  let resolution = u.scaleAndResolution.zw;
  let cellSize = u.cellAndGlyphs.xy;
  let numGlyphs = u.cellAndGlyphs.z;

  // Pixel position on the canvas and which cell it falls in.
  let pixelPos = in.uv * resolution;
  let cellCoord = floor(pixelPos / cellSize);
  let cellCenterUV = (cellCoord + vec2f(0.5)) * cellSize / resolution;

  // Sample the camera at the cell center, cover-fit.
  let camUV = vec2f(0.5) + (cellCenterUV - vec2f(0.5)) * uvScale;
  let cellColor = textureSampleBaseClampToEdge(srcTex, srcSampler, camUV).rgb;
  let lum = clamp(dot(cellColor, vec3f(0.299, 0.587, 0.114)), 0.0, 1.0);

  // Map luminance to glyph index in the atlas (dark -> sparse, light -> dense).
  let glyphIdx = clamp(floor(lum * numGlyphs), 0.0, numGlyphs - 1.0);

  // Sample the atlas at the in-cell UV, offset into the glyph's strip slot.
  let inCellUV = fract(pixelPos / cellSize);
  let atlasUV = vec2f((glyphIdx + inCellUV.x) / numGlyphs, inCellUV.y);
  let glyph = textureSample(atlasTex, atlasSampler, atlasUV).r;

  return vec4f(cellColor * glyph, 1.0);
}
`;

interface PipelineState {
  pipeline: GPURenderPipeline;
  srcSampler: GPUSampler;
  atlasView: GPUTextureView;
  atlasSampler: GPUSampler;
  uniformBuffer: GPUBuffer;
}

const loadAtlas = async (device: GPUDevice): Promise<GPUTexture> => {
  const response = await fetchAsset(require("../assets/ascii-atlas.png"));
  const buffer = await response.arrayBuffer();
  const bitmap = await createImageBitmap(buffer);
  const texture = device.createTexture({
    size: [bitmap.width, bitmap.height, 1],
    format: "rgba8unorm",
    usage:
      GPUTextureUsage.TEXTURE_BINDING |
      GPUTextureUsage.COPY_DST |
      GPUTextureUsage.RENDER_ATTACHMENT,
  });
  device.queue.copyExternalImageToTexture({ source: bitmap }, { texture }, [
    bitmap.width,
    bitmap.height,
  ]);
  return texture;
};

export const AsciiCamera = () => {
  const { element } = useWebGPUCamera<PipelineState>({
    setup: async ({ device, presentationFormat }) => {
      const atlas = await loadAtlas(device);
      const module = device.createShaderModule({ code: SHADER });
      const pipeline = device.createRenderPipeline({
        layout: "auto",
        vertex: { module, entryPoint: "vs_main" },
        fragment: {
          module,
          entryPoint: "fs_main",
          targets: [{ format: presentationFormat }],
        },
        primitive: { topology: "triangle-list" },
      });
      const srcSampler = device.createSampler({
        magFilter: "linear",
        minFilter: "linear",
      });
      // Nearest sampling on the atlas keeps glyph edges crisp instead of
      // bleeding adjacent slots in the strip.
      const atlasSampler = device.createSampler({
        magFilter: "nearest",
        minFilter: "nearest",
      });
      const uniformBuffer = device.createBuffer({
        size: 32,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
      });
      return {
        pipeline,
        srcSampler,
        atlasView: atlas.createView(),
        atlasSampler,
        uniformBuffer,
      };
    },
    render: ({
      device,
      context,
      externalTexture,
      canvasWidth,
      canvasHeight,
      frameWidth,
      frameHeight,
      pipelineState,
    }) => {
      "worklet";
      const { pipeline, srcSampler, atlasView, atlasSampler, uniformBuffer } =
        pipelineState;

      const canvasAR = canvasWidth / canvasHeight;
      const frameAR = frameWidth / frameHeight;
      let sx = 1;
      let sy = 1;
      if (frameAR > canvasAR) {
        sx = canvasAR / frameAR;
      } else {
        sy = frameAR / canvasAR;
      }
      device.queue.writeBuffer(
        uniformBuffer,
        0,
        new Float32Array([
          sx,
          sy,
          canvasWidth,
          canvasHeight,
          CELL_PX_W,
          CELL_PX_H,
          ATLAS_GLYPH_COUNT,
          0,
        ]),
      );

      const bindGroup = device.createBindGroup({
        layout: pipeline.getBindGroupLayout(0),
        entries: [
          { binding: 0, resource: externalTexture },
          { binding: 1, resource: srcSampler },
          { binding: 2, resource: atlasView },
          { binding: 3, resource: atlasSampler },
          { binding: 4, resource: { buffer: uniformBuffer } },
        ],
      });

      const encoder = device.createCommandEncoder();
      const pass = encoder.beginRenderPass({
        colorAttachments: [
          {
            view: context.getCurrentTexture().createView(),
            clearValue: { r: 0, g: 0, b: 0, a: 1 },
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      });
      pass.setPipeline(pipeline);
      pass.setBindGroup(0, bindGroup);
      pass.draw(3);
      pass.end();
      device.queue.submit([encoder.finish()]);
      context.present();
    },
  });
  return element;
};
