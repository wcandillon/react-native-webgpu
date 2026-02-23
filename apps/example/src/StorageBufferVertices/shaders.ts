export const shaderCode = /* wgsl */ `
  struct OurStruct {
    color: vec4f,
    offset: vec2f,
  };

  struct OtherStruct {
    scale: vec2f,
  };

  struct Vertex {
    position: vec2f,
  };

  struct VSOutput {
    @builtin(position) position: vec4f,
    @location(0) color: vec4f,
  };

  @group(0) @binding(0) var<storage, read> ourStructs: array<OurStruct>;
  @group(0) @binding(1) var<storage, read> otherStructs: array<OtherStruct>;
  @group(0) @binding(2) var<storage, read> pos: array<Vertex>;

  @vertex fn vs(
    @builtin(vertex_index) vertexIndex : u32,
    @builtin(instance_index) instanceIndex: u32
  ) -> VSOutput {
    let otherStruct = otherStructs[instanceIndex];
    let ourStruct = ourStructs[instanceIndex];

    var vsOut: VSOutput;
    vsOut.position = vec4f(
        pos[vertexIndex].position * otherStruct.scale + ourStruct.offset, 0.0, 1.0);
    vsOut.color = ourStruct.color;
    return vsOut;
  }

  @fragment fn fs(vsOut: VSOutput) -> @location(0) vec4f {
    return vsOut.color;
  }
`;
