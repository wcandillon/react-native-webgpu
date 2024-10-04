/* eslint-disable max-len */
export const vertexShadowWGSL = /* wgsl */ `struct Scene {
  lightViewProjMatrix: mat4x4f,
  cameraViewProjMatrix: mat4x4f,
  lightPos: vec3f,
}

struct Model {
  modelMatrix: mat4x4f,
}

@group(0) @binding(0) var<uniform> scene : Scene;
@group(1) @binding(0) var<uniform> model : Model;

@vertex
fn main(
  @location(0) position: vec3f
) -> @builtin(position) vec4f {
  return scene.lightViewProjMatrix * model.modelMatrix * vec4(position, 1.0);
}
`;

export const vertexWGSL = /* wgsl */ `struct Scene {
  lightViewProjMatrix: mat4x4f,
  cameraViewProjMatrix: mat4x4f,
  lightPos: vec3f,
}

struct Model {
  modelMatrix: mat4x4f,
}

@group(0) @binding(0) var<uniform> scene : Scene;
@group(1) @binding(0) var<uniform> model : Model;

struct VertexOutput {
  @location(0) shadowPos: vec3f,
  @location(1) fragPos: vec3f,
  @location(2) fragNorm: vec3f,

  @builtin(position) Position: vec4f,
}

@vertex
fn main(
  @location(0) position: vec3f,
  @location(1) normal: vec3f
) -> VertexOutput {
  var output : VertexOutput;

  // XY is in (-1, 1) space, Z is in (0, 1) space
  let posFromLight = scene.lightViewProjMatrix * model.modelMatrix * vec4(position, 1.0);

  // Convert XY to (0, 1)
  // Y is flipped because texture coords are Y-down.
  output.shadowPos = vec3(
    posFromLight.xy * vec2(0.5, -0.5) + vec2(0.5),
    posFromLight.z
  );

  output.Position = scene.cameraViewProjMatrix * model.modelMatrix * vec4(position, 1.0);
  output.fragPos = output.Position.xyz;
  output.fragNorm = normal;
  return output;
}`;

export const fragmentWGSL = /* wgsl */ `override shadowDepthTextureSize: f32 = 1024.0;

struct Scene {
  lightViewProjMatrix : mat4x4f,
  cameraViewProjMatrix : mat4x4f,
  lightPos : vec3f,
}

@group(0) @binding(0) var<uniform> scene : Scene;
@group(0) @binding(1) var shadowMap: texture_depth_2d;
@group(0) @binding(2) var shadowSampler: sampler_comparison;

struct FragmentInput {
  @location(0) shadowPos : vec3f,
  @location(1) fragPos : vec3f,
  @location(2) fragNorm : vec3f,
}

const albedo = vec3f(0.9);
const ambientFactor = 0.2;

@fragment
fn main(input : FragmentInput) -> @location(0) vec4f {
  // Percentage-closer filtering. Sample texels in the region
  // to smooth the result.
  var visibility = 0.0;
  let oneOverShadowDepthTextureSize = 1.0 / shadowDepthTextureSize;
  for (var y = -1; y <= 1; y++) {
    for (var x = -1; x <= 1; x++) {
      let offset = vec2f(vec2(x, y)) * oneOverShadowDepthTextureSize;

      visibility += textureSampleCompare(
        shadowMap, shadowSampler,
        input.shadowPos.xy + offset, input.shadowPos.z - 0.007
      );
    }
  }
  visibility /= 9.0;

  let lambertFactor = max(dot(normalize(scene.lightPos - input.fragPos), normalize(input.fragNorm)), 0.0);
  let lightingFactor = min(ambientFactor + visibility * lambertFactor, 1.0);

  return vec4(lightingFactor * albedo, 1.0);
}
`;

export const vertexWriteGBuffers = /* wgsl */ `struct Uniforms {
  modelMatrix : mat4x4f,
  normalModelMatrix : mat4x4f,
}
struct Camera {
  viewProjectionMatrix : mat4x4f,
  invViewProjectionMatrix : mat4x4f,
}
@group(0) @binding(0) var<uniform> uniforms : Uniforms;
@group(0) @binding(1) var<uniform> camera : Camera;

struct VertexOutput {
  @builtin(position) Position : vec4f,
  @location(0) fragNormal: vec3f,    // normal in world space
  @location(1) fragUV: vec2f,
}

@vertex
fn main(
  @location(0) position : vec3f,
  @location(1) normal : vec3f,
  @location(2) uv : vec2f
) -> VertexOutput {
  var output : VertexOutput;
  let worldPosition = (uniforms.modelMatrix * vec4(position, 1.0)).xyz;
  output.Position = camera.viewProjectionMatrix * vec4(worldPosition, 1.0);
  output.fragNormal = normalize((uniforms.normalModelMatrix * vec4(normal, 1.0)).xyz);
  output.fragUV = uv;
  return output;
}`;

export const fragmentWriteGBuffers = /* wgsl */ `struct GBufferOutput {
  @location(0) normal : vec4f,

  // Textures: diffuse color, specular color, smoothness, emissive etc. could go here
  @location(1) albedo : vec4f,
}

@fragment
fn main(
  @location(0) fragNormal: vec3f,
  @location(1) fragUV : vec2f
) -> GBufferOutput {
  // faking some kind of checkerboard texture
  let uv = floor(30.0 * fragUV);
  let c = 0.2 + 0.5 * ((uv.x + uv.y) - 2.0 * floor((uv.x + uv.y) / 2.0));

  var output : GBufferOutput;
  output.normal = vec4(normalize(fragNormal), 1.0);
  output.albedo = vec4(c, c, c, 1.0);

  return output;
}
`;

export const vertexTextureQuad = /* wgsl */ `@vertex
fn main(
  @builtin(vertex_index) VertexIndex : u32
) -> @builtin(position) vec4f {
  const pos = array(
    vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0),
    vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
  );

  return vec4f(pos[VertexIndex], 0.0, 1.0);
}
`;

export const fragmentGBuffersDebugView = /* wgsl */ `@group(0) @binding(0) var gBufferNormal: texture_2d<f32>;
@group(0) @binding(1) var gBufferAlbedo: texture_2d<f32>;
@group(0) @binding(2) var gBufferDepth: texture_depth_2d;

override canvasSizeWidth: f32;
override canvasSizeHeight: f32;

@fragment
fn main(
  @builtin(position) coord : vec4f
) -> @location(0) vec4f {
  var result : vec4f;
  let c = coord.xy / vec2f(canvasSizeWidth, canvasSizeHeight);
  if (c.x < 0.33333) {
    let rawDepth = textureLoad(
      gBufferDepth,
      vec2i(floor(coord.xy)),
      0
    );
    // remap depth into something a bit more visible
    let depth = (1.0 - rawDepth) * 50.0;
    result = vec4(depth);
  } else if (c.x < 0.66667) {
    result = textureLoad(
      gBufferNormal,
      vec2i(floor(coord.xy)),
      0
    );
    result.x = (result.x + 1.0) * 0.5;
    result.y = (result.y + 1.0) * 0.5;
    result.z = (result.z + 1.0) * 0.5;
  } else {
    result = textureLoad(
      gBufferAlbedo,
      vec2i(floor(coord.xy)),
      0
    );
  }
  return result;
}
`;

export const fragmentDeferredRendering = /* wgsl */ `@group(0) @binding(0) var gBufferNormal: texture_2d<f32>;
@group(0) @binding(1) var gBufferAlbedo: texture_2d<f32>;
@group(0) @binding(2) var gBufferDepth: texture_depth_2d;

struct LightData {
  position : vec4f,
  color : vec3f,
  radius : f32,
}
struct LightsBuffer {
  lights: array<LightData>,
}
@group(1) @binding(0) var<storage, read> lightsBuffer: LightsBuffer;

struct Config {
  numLights : u32,
}
struct Camera {
  viewProjectionMatrix : mat4x4f,
  invViewProjectionMatrix : mat4x4f,
}
@group(1) @binding(1) var<uniform> config: Config;
@group(1) @binding(2) var<uniform> camera: Camera;

fn world_from_screen_coord(coord : vec2f, depth_sample: f32) -> vec3f {
  // reconstruct world-space position from the screen coordinate.
  let posClip = vec4(coord.x * 2.0 - 1.0, (1.0 - coord.y) * 2.0 - 1.0, depth_sample, 1.0);
  let posWorldW = camera.invViewProjectionMatrix * posClip;
  let posWorld = posWorldW.xyz / posWorldW.www;
  return posWorld;
}

@fragment
fn main(
  @builtin(position) coord : vec4f
) -> @location(0) vec4f {
  var result : vec3f;

  let depth = textureLoad(
    gBufferDepth,
    vec2i(floor(coord.xy)),
    0
  );

  // Don't light the sky.
  if (depth >= 1.0) {
    discard;
  }

  let bufferSize = textureDimensions(gBufferDepth);
  let coordUV = coord.xy / vec2f(bufferSize);
  let position = world_from_screen_coord(coordUV, depth);

  let normal = textureLoad(
    gBufferNormal,
    vec2i(floor(coord.xy)),
    0
  ).xyz;

  let albedo = textureLoad(
    gBufferAlbedo,
    vec2i(floor(coord.xy)),
    0
  ).rgb;

  for (var i = 0u; i < config.numLights; i++) {
    let L = lightsBuffer.lights[i].position.xyz - position;
    let distance = length(L);
    if (distance > lightsBuffer.lights[i].radius) {
      continue;
    }
    let lambert = max(dot(normal, normalize(L)), 0.0);
    result += vec3f(
      lambert * pow(1.0 - distance / lightsBuffer.lights[i].radius, 2.0) * lightsBuffer.lights[i].color * albedo
    );
  }

  // some manual ambient
  result += vec3(0.2);

  return vec4(result, 1.0);
}
`;

export const lightUpdate = /* wgsl */ `struct LightData {
  position : vec4f,
  color : vec3f,
  radius : f32,
}
struct LightsBuffer {
  lights: array<LightData>,
}
@group(0) @binding(0) var<storage, read_write> lightsBuffer: LightsBuffer;

struct Config {
  numLights : u32,
}
@group(0) @binding(1) var<uniform> config: Config;

struct LightExtent {
  min : vec4f,
  max : vec4f,
}
@group(0) @binding(2) var<uniform> lightExtent: LightExtent;

@compute @workgroup_size(64, 1, 1)
fn main(@builtin(global_invocation_id) GlobalInvocationID : vec3u) {
  var index = GlobalInvocationID.x;
  if (index >= config.numLights) {
    return;
  }

  lightsBuffer.lights[index].position.y = lightsBuffer.lights[index].position.y - 0.5 - 0.003 * (f32(index) - 64.0 * floor(f32(index) / 64.0));

  if (lightsBuffer.lights[index].position.y < lightExtent.min.y) {
    lightsBuffer.lights[index].position.y = lightExtent.max.y;
  }
}
`;
