// Backdrop shader. Fullscreen triangle that samples the pre-blurred camera
// image (cover-fit baked in by the prepass), dims it ~50%, and adds a soft
// vignette so the chrome sphere stays the focal point of the scene.
export const BACKDROP_SHADER = /* wgsl */ `
@group(0) @binding(0) var src: texture_2d<f32>;
@group(0) @binding(1) var samp: sampler;

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
  var c = textureSampleLevel(src, samp, in.uv, 0.0).rgb;
  // Subdue the room: half-brightness + soft vignette darkening the edges by
  // another ~40%. Keeps a sense of ambient lighting without competing with
  // the chrome reflection.
  c = c * 0.55;
  let d = distance(in.uv, vec2f(0.5));
  let v = 1.0 - smoothstep(0.4, 0.95, d) * 0.45;
  return vec4f(c * v, 1.0);
}
`;

// Chrome-style env-map reflection driven by the live camera feed.
//
// For each fragment we compute the reflection vector around the world-space
// normal, convert it to spherical (theta, phi) → uv, and sample the external
// camera texture. textureSampleBaseClampToEdge is the only sampling call
// allowed on texture_external, but it's all we need: spherical uvs land
// inside [0, 1]^2 so no wrap mode tricks are required.
//
// The visible result is the classic CGI chrome ball: your face wraps around
// the sphere as if the camera image were the ambient environment, and the
// cube + torus reflect distorted copies of the same scene.

export const SHADER = /* wgsl */ `
struct Scene {
  viewProj: mat4x4f,
  cameraPos: vec4f,   // xyz = world-space eye, w unused
  lightDir: vec4f,    // xyz = unit vector toward light, w unused
};

struct Object {
  model: mat4x4f,
};

@group(0) @binding(0) var<uniform> scene: Scene;
@group(0) @binding(1) var<uniform> obj: Object;
@group(0) @binding(2) var srcTex: texture_external;
@group(0) @binding(3) var srcSampler: sampler;

struct VsIn {
  @location(0) position: vec3f,
  @location(1) normal: vec3f,
};

struct VsOut {
  @builtin(position) clipPos: vec4f,
  @location(0) worldPos: vec3f,
  @location(1) worldNormal: vec3f,
};

@vertex
fn vs_main(in: VsIn) -> VsOut {
  let world = obj.model * vec4f(in.position, 1.0);
  // Rotation-only transforms: normal matrix is the upper-left 3x3 of model.
  // No non-uniform scale anywhere in the scene, so inverse-transpose collapses
  // to this.
  let normalMat = mat3x3f(
    obj.model[0].xyz,
    obj.model[1].xyz,
    obj.model[2].xyz,
  );
  var out: VsOut;
  out.clipPos = scene.viewProj * world;
  out.worldPos = world.xyz;
  out.worldNormal = normalize(normalMat * in.normal);
  return out;
}

const PI: f32 = 3.14159265359;

fn sphericalUv(dir: vec3f) -> vec2f {
  // dir is treated as a direction from the chrome surface to the
  // environment. theta wraps around the y-axis, phi runs pole to pole.
  let theta = atan2(dir.z, dir.x);
  let phi = acos(clamp(dir.y, -1.0, 1.0));
  return vec2f((theta + PI) / (2.0 * PI), phi / PI);
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4f {
  let n = normalize(in.worldNormal);
  let v = normalize(scene.cameraPos.xyz - in.worldPos);
  // reflect() expects the incident vector (from light/source to surface), so
  // negate the view direction.
  let r = normalize(reflect(-v, n));

  let uv = sphericalUv(r);
  let env = textureSampleBaseClampToEdge(srcTex, srcSampler, uv).rgb;

  // Schlick-ish Fresnel: chrome reflectance is high everywhere, but boost a
  // bit at grazing angles so silhouettes catch the light.
  let cosTheta = max(dot(n, v), 0.0);
  let fresnel = pow(1.0 - cosTheta, 3.0);
  let tint = vec3f(0.96, 0.97, 1.00); // subtle cool cast, very polished chrome
  var color = env * tint + vec3f(fresnel) * 0.18;

  // Single directional specular highlight so the chrome doesn't look like a
  // pure billboard. Halfway vector with a tight exponent.
  let h = normalize(scene.lightDir.xyz + v);
  let spec = pow(max(dot(n, h), 0.0), 96.0);
  color = color + vec3f(spec) * 1.2;

  return vec4f(color, 1.0);
}
`;
