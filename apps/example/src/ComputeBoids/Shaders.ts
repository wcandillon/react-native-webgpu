const triangleSize = 0.03;

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

  struct VertexOutput {
    @builtin(position) position : vec4f,
    @location(1) color : vec4f,
  };

  @vertex
  fn mainVert(@builtin(instance_index) ii: u32, @location(0) v: vec2f) -> VertexOutput {
    let instanceInfo = _EXT_.trianglePos[ii];

    let angle = getRotationFromVelocity(instanceInfo.velocity);
    let rotated = rotate(v, angle);

    let offset = instanceInfo.position;
    let pos = vec4(rotated + offset, 0.0, 1.0);

    let color = vec4(
        sin(angle + _EXT_.colorPalette.r) * 0.45 + 0.45,
        sin(angle + _EXT_.colorPalette.g) * 0.45 + 0.45,
        sin(angle + _EXT_.colorPalette.b) * 0.45 + 0.45,
        1.0);

    return VertexOutput(pos, color);
  }

  @fragment
  fn mainFrag(@location(1) color : vec4f) -> @location(0) vec4f {
    return color;
  }
`;

export const computeCode = /* wgsl */ `
  @compute @workgroup_size(1)
  fn mainCompute(@builtin(global_invocation_id) gid: vec3u) {
    let index = gid.x;
    var instanceInfo = _EXT_.currentTrianglePos[index];
    var separation = vec2(0.0, 0.0);
    var alignment = vec2(0.0, 0.0);
    var alignmentCount = 0u;
    var cohesion = vec2(0.0, 0.0);
    var cohesionCount = 0u;
    for (var i = 0u; i < arrayLength(&_EXT_.currentTrianglePos); i = i + 1) {
      if (i == index) {
        continue;
      }
      var other = _EXT_.currentTrianglePos[i];
      var dist = distance(instanceInfo.position, other.position);
      if (dist < _EXT_.params.separationDistance) {
        separation += instanceInfo.position - other.position;
      }
      if (dist < _EXT_.params.alignmentDistance) {
        alignment += other.velocity;
        alignmentCount++;
      }
      if (dist < _EXT_.params.cohesionDistance) {
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
    instanceInfo.velocity +=
      (separation * _EXT_.params.separationStrength)
      + (alignment * _EXT_.params.alignmentStrength)
      + (cohesion * _EXT_.params.cohesionStrength);
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
    _EXT_.nextTrianglePos[index] = instanceInfo;
  }
`;
