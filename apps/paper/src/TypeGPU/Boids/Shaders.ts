const triangleAmount = 1000;
const triangleSize = 0.02;

export const renderCode = /* wgsl */ `
  fn rotate(v: vec2f, angle: f32) -> vec2f {
    let pos = vec2(
      (v.x * cos(angle)) - (v.y * sin(angle)),
      (v.x * sin(angle)) + (v.y * cos(angle))
    );
    return pos;
  };
  fn getRotationFromVelocity(velocity: vec2f) -> f32 {
    return -atan2(velocity.x, velocity.y);
  };
  struct TriangleData {
    position : vec2f,
    velocity : vec2f,
  };
  struct VertexOutput {
    @builtin(position) position : vec4f,
    @location(1) fragUV : vec2f,
  };
  @binding(0) @group(0) var<uniform> trianglePos : array<TriangleData, ${triangleAmount}>;
  @vertex
  fn mainVert(@builtin(instance_index) ii: u32 ,@location(0) v: vec2f) -> VertexOutput {
    let instanceInfo = trianglePos[ii];
    let rotated = rotate(v, getRotationFromVelocity(instanceInfo.velocity));
    let offset = instanceInfo.position;
    let pos = vec4(rotated + offset, 0.0, 1.0);
    let fragUV = (rotated + vec2f(${triangleSize}, ${triangleSize})) / vec2f(${triangleSize} * 2.0);
    return VertexOutput(pos, fragUV);
  }
  @fragment
  fn mainFrag(@location(1) fragUV : vec2f) -> @location(0) vec4f {
    let color1 = vec3(196.0 / 255.0, 100.0 / 255.0, 255.0 / 255.0);
    let color2 = vec3(29.0 / 255.0, 114.0 / 255.0, 240.0 / 255.0);
    let dist = length(fragUV - vec2(0.5, 0.5));
    let color = mix(color1, color2, dist);
    return vec4(color, 1.0);
  }
`;

export const computeCode = /* wgsl */ `
  struct TriangleData {
    position : vec2f,
    velocity : vec2f,
  };
  struct Parameters {
    separation_distance : f32,
    separation_strength : f32,
    alignment_distance : f32,
    alignment_strength : f32,
    cohesion_distance : f32,
    cohesion_strength : f32,
  };
  @binding(0) @group(0) var<uniform> currentTrianglePos : array<TriangleData, ${triangleAmount}>;
  @binding(1) @group(0) var<storage, read_write> nextTrianglePos : array<TriangleData>;
  @binding(2) @group(0) var<storage> params : Parameters;
  @compute @workgroup_size(1)
  fn mainCompute(@builtin(global_invocation_id) gid: vec3u) {
    let index = gid.x;
    var instanceInfo = currentTrianglePos[index];
    var separation = vec2(0.0, 0.0);
    var alignment = vec2(0.0, 0.0);
    var alignmentCount = 0u;
    var cohesion = vec2(0.0, 0.0);
    var cohesionCount = 0u;
    for (var i = 0u; i < ${triangleAmount}; i = i + 1) {
      if (i == index) {
        continue;
      }
      var other = currentTrianglePos[i];
      var dist = distance(instanceInfo.position, other.position);
      if (dist < params.separation_distance) {
        separation += instanceInfo.position - other.position;
      }
      if (dist < params.alignment_distance) {
        alignment += other.velocity;
        alignmentCount++;
      }
      if (dist < params.cohesion_distance) {
        cohesion += other.position;
        cohesionCount++;
      }
    };
    if (alignmentCount > 0u) {
      alignment = alignment / f32(alignmentCount);
    }
    if (cohesionCount > 0u) {
      cohesion = (cohesion / f32(cohesionCount)) - instanceInfo.position;
    }
    instanceInfo.velocity += (separation * params.separation_strength) + (alignment * params.alignment_strength) + (cohesion * params.cohesion_strength);
    instanceInfo.velocity = normalize(instanceInfo.velocity) * clamp(length(instanceInfo.velocity), 0.0, 0.01);
    let triangleSize = ${triangleSize};
    if (instanceInfo.position[0] > 1.0 + triangleSize) {
      instanceInfo.position[0] = -1.0 - triangleSize;
    }
    if (instanceInfo.position[1] > 1.0 + triangleSize) {
      instanceInfo.position[1] = -1.0 - triangleSize;
    }
    if (instanceInfo.position[0] < -1.0 - triangleSize) {
      instanceInfo.position[0] = 1.0 + triangleSize;
    }
    if (instanceInfo.position[1] < -1.0 - triangleSize) {
      instanceInfo.position[1] = 1.0 + triangleSize;
    }
    instanceInfo.position += instanceInfo.velocity;
    nextTrianglePos[index] = instanceInfo;
  }
`;
