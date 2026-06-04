import React, { useCallback, useMemo, useRef } from "react";
import type { TgpuBuffer } from "typegpu";
import tgpu, { d, std } from "typegpu";
import { StyleSheet, View, Text, Button } from "react-native";
import { Canvas } from "react-native-webgpu";
import {
  useBindGroup,
  useBuffer,
  useConfigureContext,
  useFrame,
  useRoot,
  useUniform,
} from "@typegpu/react";

function rotate(v: d.v2f, angle: number): d.v2f {
  "use gpu";
  let pos = d.vec2f(
    v.x * std.cos(angle) - v.y * std.sin(angle),
    v.x * std.sin(angle) + v.y * std.cos(angle),
  );
  return pos;
}

function getRotationFromVelocity(velocity: d.v2f): number {
  "use gpu";
  return -std.atan2(velocity.x, velocity.y);
}

const PhysicsParams = d.struct({
  separationDistance: d.f32,
  separationStrength: d.f32,
  alignmentDistance: d.f32,
  alignmentStrength: d.f32,
  cohesionDistance: d.f32,
  cohesionStrength: d.f32,
});
type PhysicsParams = d.InferGPU<typeof PhysicsParams>;

const BoidData = d.struct({
  position: d.vec2f,
  velocity: d.vec2f,
});

const BoidDataArray = d.arrayOf(BoidData);

const renderLayout = tgpu.bindGroupLayout({
  boids: { storage: BoidDataArray },
});

const computeLayout = tgpu.bindGroupLayout({
  params: { uniform: PhysicsParams },
  currentBoids: { storage: BoidDataArray },
  nextBoids: { storage: BoidDataArray, access: "mutable" },
});

const colorPresets = {
  plumTree: d.vec3f(1.0, 2.0, 1.0),
  jeans: d.vec3f(2.0, 1.5, 1.0),
  greyscale: d.vec3f(0, 0, 0),
  hotcold: d.vec3f(0, 3.14, 3.14),
} as const;
type ColorPreset = keyof typeof colorPresets;

const presets = {
  default: {
    separationDistance: 0.05,
    separationStrength: 0.001,
    alignmentDistance: 0.3,
    alignmentStrength: 0.01,
    cohesionDistance: 0.3,
    cohesionStrength: 0.001,
  },
  mosquitos: {
    separationDistance: 0.02,
    separationStrength: 0.01,
    alignmentDistance: 0.0,
    alignmentStrength: 0.0,
    cohesionDistance: 0.177,
    cohesionStrength: 0.011,
  },
  blobs: {
    separationDistance: 0.033,
    separationStrength: 0.051,
    alignmentDistance: 0.047,
    alignmentStrength: 0.1,
    cohesionDistance: 0.3,
    cohesionStrength: 0.013,
  },
  particles: {
    separationDistance: 0.035,
    separationStrength: 1,
    alignmentDistance: 0.0,
    alignmentStrength: 0.0,
    cohesionDistance: 0.0,
    cohesionStrength: 0.0,
  },
} as const;

const triangleSize = 0.03;

const triangleVertices = tgpu.const(d.arrayOf(d.vec2f), [
  d.vec2f(0, triangleSize),
  d.vec2f(-triangleSize / 2, -triangleSize / 2),
  d.vec2f(triangleSize / 2, -triangleSize / 2),
]);

const boidCount = 1000;

function randomizeBoids(buffer: TgpuBuffer<ReturnType<typeof BoidDataArray>>) {
  const data = Array.from({ length: boidCount }, () => ({
    position: d.vec2f(Math.random() * 2 - 1, Math.random() * 2 - 1),
    velocity: d.vec2f(Math.random() * 0.1 - 0.05, Math.random() * 0.1 - 0.05),
  }));

  buffer.write(data);
}

function simulate(boidIdx: number) {
  "use gpu";
  // eslint-disable-next-line prefer-destructuring
  const params = computeLayout.$.params;
  // eslint-disable-next-line prefer-destructuring
  const currentBoids = computeLayout.$.currentBoids;

  const instanceInfo = BoidData(currentBoids[boidIdx]);
  let separation = d.vec2f();
  let alignment = d.vec2f();
  let alignmentCount = d.u32(0);
  let cohesion = d.vec2f();
  let cohesionCount = d.u32(0);

  for (let i = 0; i < currentBoids.length; i++) {
    if (i === boidIdx) {
      continue;
    }
    const other = currentBoids[i];
    const dist = std.distance(instanceInfo.position, other.position);
    if (dist < params.separationDistance) {
      separation = separation.add(instanceInfo.position.sub(other.position));
    }
    if (dist < params.alignmentDistance) {
      alignment = alignment.add(other.velocity);
      alignmentCount++;
    }
    if (dist < params.cohesionDistance) {
      cohesion = cohesion.add(other.position);
      cohesionCount++;
    }
  }
  if (alignmentCount > 0) {
    alignment = alignment.div(alignmentCount);
  }
  if (cohesionCount > 0) {
    cohesion = cohesion.div(cohesionCount).sub(instanceInfo.position);
  }
  instanceInfo.velocity = instanceInfo.velocity.add(
    separation
      .mul(params.separationStrength)
      .add(alignment.mul(params.alignmentStrength))
      .add(cohesion.mul(params.cohesionStrength)),
  );
  instanceInfo.velocity = std
    .normalize(instanceInfo.velocity)
    .mul(std.clamp(std.length(instanceInfo.velocity), 0.0, 0.01));

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
  instanceInfo.position = instanceInfo.position.add(instanceInfo.velocity);
  computeLayout.$.nextBoids[boidIdx] = BoidData(instanceInfo);
}

export function ComputeBoids() {
  const root = useRoot();

  const colorPalette = useUniform(d.vec3f, { initial: colorPresets.jeans });
  const physicsParams = useUniform(PhysicsParams, { initial: presets.default });

  // Storing the state in two buffers, and swapping between which one is the input and which one is the output.
  // Also known as "double-buffering"
  const boidsA = useBuffer(BoidDataArray(boidCount), {
    initial: randomizeBoids,
  }).$usage("storage");
  const boidsB = useBuffer(BoidDataArray(boidCount)).$usage("storage");

  const computePipeline = useMemo(() => {
    return root.createGuardedComputePipeline(simulate);
  }, [root]);

  const renderPipeline = useMemo(() => {
    return root.createRenderPipeline({
      vertex: ({ $instanceIndex: boidIdx, $vertexIndex: vi }) => {
        "use gpu";
        const boid = renderLayout.$.boids[boidIdx];

        const angle = getRotationFromVelocity(boid.velocity);

        const local = triangleVertices.$[vi];
        const rotated = rotate(local, angle);
        const pos = rotated.add(boid.position);

        const color = std.sin(colorPalette.$.add(angle)).mul(0.45).add(0.45);

        return {
          $position: d.vec4f(pos, 0, 1),
          color: color,
        };
      },
      fragment: ({ color }) => {
        "use gpu";
        return d.vec4f(color, 1);
      },
    });
  }, [root, colorPalette]);

  const updateColorPreset = useCallback(
    (preset: ColorPreset) => {
      colorPalette.write(colorPresets[preset]);
    },
    [colorPalette],
  );

  const { ref, ctxRef } = useConfigureContext({ alphaMode: "premultiplied" });

  const renderGroupA = useBindGroup(renderLayout, { boids: boidsA });
  const renderGroupB = useBindGroup(renderLayout, { boids: boidsB });

  const computeGroupA = useBindGroup(computeLayout, {
    params: physicsParams.buffer,
    currentBoids: boidsA,
    nextBoids: boidsB,
  });
  const computeGroupB = useBindGroup(computeLayout, {
    params: physicsParams.buffer,
    currentBoids: boidsB,
    nextBoids: boidsA,
  });

  // Used to swap between A and B variants of resources
  const evenRef = useRef(false);

  const handleRandomize = useCallback(() => {
    randomizeBoids(evenRef.current ? boidsB : boidsA);
  }, [boidsA, boidsB]);

  useFrame(() => {
    const ctx = ctxRef.current;
    if (!ctx) return;

    evenRef.current = !evenRef.current;

    computePipeline
      .with(evenRef.current ? computeGroupA : computeGroupB)
      .dispatchThreads(boidCount);

    renderPipeline
      .with(evenRef.current ? renderGroupB : renderGroupA)
      .withColorAttachment({ view: ctx })
      .draw(3, boidCount);

    ctx.present?.();
  });

  return (
    <View style={style.container}>
      <Canvas ref={ref} style={style.webgpu} />
      <View style={style.controls}>
        <View style={style.buttonRow}>
          <Text style={style.spanText}>randomize: </Text>
          <Button title="🔀" onPress={handleRandomize} />
        </View>
        <View style={style.buttonRow}>
          <Text style={style.spanText}>presets: </Text>
          <Button
            title="🐦"
            onPress={() => physicsParams.write(presets.default)}
          />
          <Button
            title="🦟"
            onPress={() => physicsParams.write(presets.mosquitos)}
          />
          <Button
            title="💧"
            onPress={() => physicsParams.write(presets.blobs)}
          />
          <Button
            title="⚛️"
            onPress={() => physicsParams.write(presets.particles)}
          />
        </View>
        <View style={style.buttonRow}>
          <Text style={style.spanText}>colors: </Text>
          <Button title="🟪🟩" onPress={() => updateColorPreset("plumTree")} />
          <Button title="🟦🟫" onPress={() => updateColorPreset("jeans")} />
          <Button title="⬛️⬜️" onPress={() => updateColorPreset("greyscale")} />
          <Button title="🟥🟦" onPress={() => updateColorPreset("hotcold")} />
        </View>
      </View>
    </View>
  );
}

const style = StyleSheet.create({
  container: {
    flex: 1,
  },
  webgpu: {
    aspectRatio: 1,
  },
  spanText: {
    fontSize: 20,
    fontWeight: "bold",
  },
  controls: {
    flex: 1,
    justifyContent: "center",
  },
  buttonRow: {
    flexDirection: "row",
    justifyContent: "center",
    alignItems: "center",
  },
});
