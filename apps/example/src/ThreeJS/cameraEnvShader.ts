// Tiny "copy camera frame into an rgba8unorm texture" shader. The output
// texture is then wrapped in a THREE.ExternalTexture and mapped onto a
// billboarded plane that three.js' CubeCamera bakes into the helmet's
// envMap — i.e. it acts as a virtual screen at the viewer's location
// rather than a 360° panorama. The destination texture's aspect (9:16) is
// chosen to match the camera frame's post-rotation aspect so no stretching
// happens here; the fullscreen triangle just rotates+mirrors the source to
// selfie-upright.

export const CAMERA_ENV_SHADER = /* wgsl */ `
struct VsOut {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};

@group(0) @binding(0) var srcTex: texture_external;
@group(0) @binding(1) var srcSampler: sampler;

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
  // Front camera: iOS delivers landscape-orientation frames with the
  // horizontal axis already mirrored (selfie convention). To bring those
  // upright in the equirect we (a) compensate for the horizontal mirror
  // by sampling at (1-x) and (b) rotate 90° CCW with V flipped, giving
  // (1-v, 1-u). Equivalent to the 90° CW back-cam mapping (v, 1-u) with
  // its U axis pre-flipped to undo the mirror.
  let rotatedUv = vec2f(1.0 - in.uv.y, 1.0 - in.uv.x);
  let c = textureSampleBaseClampToEdge(srcTex, srcSampler, rotatedUv);
  return vec4f(c.rgb, 1.0);
}
`;
