export const opaqueWGSL = /*wgsl*/ `struct Uniforms {
  modelViewProjectionMatrix: mat4x4f,
};

@binding(0) @group(0) var<uniform> uniforms: Uniforms;

struct VertexOutput {
  @builtin(position) position: vec4f,
  @location(0) @interpolate(flat) instance: u32
};

@vertex
fn main_vs(@location(0) position: vec4f, @builtin(instance_index) instance: u32) -> VertexOutput {
  var output: VertexOutput;

  // distribute instances into a staggered 4x4 grid
  const gridWidth = 125.0;
  const cellSize = gridWidth / 4.0;
  let row = instance / 2u;
  let col = instance % 2u;

  let xOffset = -gridWidth / 2.0 + cellSize / 2.0 + 2.0 * cellSize * f32(col) + f32(row % 2u != 0u) * cellSize;
  let zOffset = -gridWidth / 2.0 + cellSize / 2.0 + 2.0 + f32(row) * cellSize;

  let offsetPos = vec4(position.x + xOffset, position.y, position.z + zOffset, position.w);

  output.position = uniforms.modelViewProjectionMatrix * offsetPos;
  output.instance = instance;
  return output;
}

@fragment
fn main_fs(@location(0) @interpolate(flat) instance: u32) -> @location(0) vec4f {
  const colors = array<vec3f,6>(
      vec3(1.0, 0.0, 0.0),
      vec3(0.0, 1.0, 0.0),
      vec3(0.0, 0.0, 1.0),
      vec3(1.0, 0.0, 1.0),
      vec3(1.0, 1.0, 0.0),
      vec3(0.0, 1.0, 1.0),
  );

  return vec4(colors[instance % 6u], 1.0);
}
`;

export const translucentWGSL = /*wgsl*/ `struct Uniforms {
  modelViewProjectionMatrix: mat4x4f,
  maxStorableFragments: u32,
  targetWidth: u32,
};

struct SliceInfo {
  sliceStartY: i32
};

struct Heads {
  numFragments: atomic<u32>,
  data: array<atomic<u32>>
};

struct LinkedListElement {
  color: vec4f,
  depth: f32,
  next: u32
};

struct LinkedList {
  data: array<LinkedListElement>
};

@binding(0) @group(0) var<uniform> uniforms: Uniforms;
@binding(1) @group(0) var<storage, read_write> heads: Heads;
@binding(2) @group(0) var<storage, read_write> linkedList: LinkedList;
@binding(3) @group(0) var opaqueDepthTexture: texture_depth_2d;
@binding(4) @group(0) var<uniform> sliceInfo: SliceInfo;

struct VertexOutput {
  @builtin(position) position: vec4f,
  @location(0) @interpolate(flat) instance: u32
};

@vertex
fn main_vs(@location(0) position: vec4f, @builtin(instance_index) instance: u32) -> VertexOutput {
  var output: VertexOutput;

  // distribute instances into a staggered 4x4 grid
  const gridWidth = 125.0;
  const cellSize = gridWidth / 4.0;
  let row = instance / 2u;
  let col = instance % 2u;

  let xOffset = -gridWidth / 2.0 + cellSize / 2.0 + 2.0 * cellSize * f32(col) + f32(row % 2u == 0u) * cellSize;
  let zOffset = -gridWidth / 2.0 + cellSize / 2.0 + 2.0 + f32(row) * cellSize;

  let offsetPos = vec4(position.x + xOffset, position.y, position.z + zOffset, position.w);

  output.position = uniforms.modelViewProjectionMatrix * offsetPos;
  output.instance = instance;

  return output;
}

@fragment
fn main_fs(@builtin(position) position: vec4f, @location(0) @interpolate(flat) instance: u32) {
  const colors = array<vec3f,6>(
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0),
    vec3(1.0, 0.0, 1.0),
    vec3(1.0, 1.0, 0.0),
    vec3(0.0, 1.0, 1.0),
  );

  let fragCoords = vec2i(position.xy);
  let opaqueDepth = textureLoad(opaqueDepthTexture, fragCoords, 0);

  // reject fragments behind opaque objects
  if position.z >= opaqueDepth {
    discard;
  }

  // The index in the heads buffer corresponding to the head data for the fragment at
  // the current location.
  let headsIndex = u32(fragCoords.y - sliceInfo.sliceStartY) * uniforms.targetWidth + u32(fragCoords.x);

  // The index in the linkedList buffer at which to store the new fragment
  let fragIndex = atomicAdd(&heads.numFragments, 1u);

  // If we run out of space to store the fragments, we just lose them
  if fragIndex < uniforms.maxStorableFragments {
    let lastHead = atomicExchange(&heads.data[headsIndex], fragIndex);
    linkedList.data[fragIndex].depth = position.z;
    linkedList.data[fragIndex].next = lastHead;
    linkedList.data[fragIndex].color = vec4(colors[(instance + 3u) % 6u], 0.3);
  }
}
`;

export const compositeWGSL = /*wgsl*/ `struct Uniforms {
  modelViewProjectionMatrix: mat4x4f,
  maxStorableFragments: u32,
  targetWidth: u32,
};

struct SliceInfo {
  sliceStartY: i32
};

struct Heads {
  numFragments: u32,
  data: array<u32>
};

struct LinkedListElement {
  color: vec4f,
  depth: f32,
  next: u32
};

struct LinkedList {
  data: array<LinkedListElement>
};

@binding(0) @group(0) var<uniform> uniforms: Uniforms;
@binding(1) @group(0) var<storage, read_write> heads: Heads;
@binding(2) @group(0) var<storage, read_write> linkedList: LinkedList;
@binding(3) @group(0) var<uniform> sliceInfo: SliceInfo;

// Output a full screen quad
@vertex
fn main_vs(@builtin(vertex_index) vertIndex: u32) -> @builtin(position) vec4f {
  const position = array<vec2f, 6>(
    vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0),
    vec2(-1.0, -1.0),
    vec2(1.0, 1.0),
    vec2(-1.0, 1.0),
  );
  
  return vec4(position[vertIndex], 0.0, 1.0);
}

@fragment
fn main_fs(@builtin(position) position: vec4f) -> @location(0) vec4f {
  let fragCoords = vec2i(position.xy);
  let headsIndex = u32(fragCoords.y - sliceInfo.sliceStartY) * uniforms.targetWidth + u32(fragCoords.x);

  // The maximum layers we can process for any pixel
  const maxLayers = 12u;

  var layers: array<LinkedListElement, maxLayers>;

  var numLayers = 0u;
  var elementIndex = heads.data[headsIndex];

  // copy the list elements into an array up to the maximum amount of layers
  while elementIndex != 0xFFFFFFFFu && numLayers < maxLayers {
    layers[numLayers] = linkedList.data[elementIndex];
    numLayers++;
    elementIndex = linkedList.data[elementIndex].next;
  }

  if numLayers == 0u {
    discard;
  }
  
  // sort the fragments by depth
  for (var i = 1u; i < numLayers; i++) {
    let toInsert = layers[i];
    var j = i;

    while j > 0u && toInsert.depth > layers[j - 1u].depth {
      layers[j] = layers[j - 1u];
      j--;
    }

    layers[j] = toInsert;
  }

  // pre-multiply alpha for the first layer
  var color = vec4(layers[0].color.a * layers[0].color.rgb, layers[0].color.a);

  // blend the remaining layers
  for (var i = 1u; i < numLayers; i++) {
    let mixed = mix(color.rgb, layers[i].color.rgb, layers[i].color.aaa);
    color = vec4(mixed, color.a);
  }

  return color;
}`;
