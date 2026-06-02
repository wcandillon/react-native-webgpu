// Two shaders that together feed the Blur effect with a true large-kernel
// gaussian, computed once on the GPU per frame and resampled by the main pass.
//
// 1. PREPASS_SHADER. Renders the external (YUV) camera texture into a regular
//    rgba8unorm texture, cover-projected at 1/4 canvas resolution. This solves
//    two problems at once:
//      a. texture_external can only be sampled with textureSampleBaseClampToEdge
//         and cannot be bound to a compute pipeline that expects texture_2d,
//         so we need a fixup pass anyway.
//      b. The blur runs at 1/4 res, and the main fragment's linear sampler
//         upsamples for free, which makes the effective kernel ~4x wider than
//         what we actually compute. Big blur, cheap.
//
// 2. BLUR_SHADER. The tile-based separable box-blur compute shader from the
//    WebGPU samples repo, used unmodified. One workgroup loads a 128 x 4 tile
//    into shared memory, then 32 threads write filterDim-averaged outputs.
//    A `flip` uniform swaps x/y so the same shader does both axes. Iterating
//    H-V passes (the variance adds) approximates a gaussian.

export const PREPASS_SHADER = /* wgsl */ `
struct PrepassUniforms {
  texSize: vec2f,
  canvasSize: vec2f,
};

@group(0) @binding(0) var srcTex: texture_external;
@group(0) @binding(1) var srcSampler: sampler;
@group(0) @binding(2) var<uniform> u: PrepassUniforms;

struct VsOut {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};

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
  // Cover-fit, same math the worklet sends via uvScale to the main pass so
  // the blurred image lines up with the unblurred path.
  let canvasAR = u.canvasSize.x / u.canvasSize.y;
  let texAR = u.texSize.x / u.texSize.y;
  var scale: vec2f;
  if (texAR > canvasAR) {
    scale = vec2f(canvasAR / texAR, 1.0);
  } else {
    scale = vec2f(1.0, texAR / canvasAR);
  }
  let uv = vec2f(0.5) + (in.uv - vec2f(0.5)) * scale;
  // cameraCoord (vertical flip) + cameraDecode (YUV->RGB) come from
  // CAMERA_PRELUDE, prepended when this module is compiled. They are no-ops on
  // iOS and handle the Android opaque-YUV case.
  let c = cameraDecode(textureSampleBaseClampToEdge(
    srcTex,
    srcSampler,
    cameraCoord(clamp(uv, vec2f(0.0), vec2f(1.0))),
  ));
  return vec4f(c.rgb, 1.0);
}
`;

export const BLUR_SHADER = /* wgsl */ `
struct Params {
  filterDim: i32,
  blockDim: u32,
}

@group(0) @binding(0) var samp: sampler;
@group(0) @binding(1) var<uniform> params: Params;
@group(1) @binding(1) var inputTex: texture_2d<f32>;
@group(1) @binding(2) var outputTex: texture_storage_2d<rgba8unorm, write>;

struct Flip {
  value: u32,
}
@group(1) @binding(3) var<uniform> flip: Flip;

var<workgroup> tile: array<array<vec3f, 128>, 4>;

@compute @workgroup_size(32, 1, 1)
fn main(
  @builtin(workgroup_id) WorkGroupID: vec3u,
  @builtin(local_invocation_id) LocalInvocationID: vec3u,
) {
  let filterOffset = (params.filterDim - 1) / 2;
  let dims = vec2i(textureDimensions(inputTex, 0));
  let baseIndex = vec2i(WorkGroupID.xy * vec2(params.blockDim, 4u) +
                          LocalInvocationID.xy * vec2(4u, 1u))
                  - vec2(filterOffset, 0);

  for (var r = 0; r < 4; r++) {
    for (var c = 0; c < 4; c++) {
      var loadIndex = baseIndex + vec2(c, r);
      if (flip.value != 0u) {
        loadIndex = loadIndex.yx;
      }
      tile[r][4u * LocalInvocationID.x + u32(c)] = textureSampleLevel(
        inputTex,
        samp,
        (vec2f(loadIndex) + vec2f(0.5)) / vec2f(dims),
        0.0,
      ).rgb;
    }
  }

  workgroupBarrier();

  for (var r = 0; r < 4; r++) {
    for (var c = 0; c < 4; c++) {
      var writeIndex = baseIndex + vec2(c, r);
      if (flip.value != 0u) {
        writeIndex = writeIndex.yx;
      }
      let center = i32(4u * LocalInvocationID.x) + c;
      if (center >= filterOffset &&
          center < 128 - filterOffset &&
          all(writeIndex < dims)) {
        var acc = vec3f(0.0);
        for (var f = 0; f < params.filterDim; f++) {
          let i = center + f - filterOffset;
          acc = acc + (1.0 / f32(params.filterDim)) * tile[r][i];
        }
        textureStore(outputTex, writeIndex, vec4f(acc, 1.0));
      }
    }
  }
}
`;
