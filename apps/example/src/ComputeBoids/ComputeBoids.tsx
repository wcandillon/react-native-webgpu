import React, { useCallback, useRef } from "react";
import tgpu from "typegpu";
import * as d from "typegpu/data";
import { StyleSheet, View, Text, Button } from "react-native";
import { Canvas } from "react-native-wgpu";

import { useWebGPU } from "../components/useWebGPU";
import { toBeAssignedLater } from "../components/utils";

import { renderCode, computeCode } from "./Shaders";

type BoidsOptions = {
  separationDistance: number;
  separationStrength: number;
  alignmentDistance: number;
  alignmentStrength: number;
  cohesionDistance: number;
  cohesionStrength: number;
};

const Parameters = d.struct({
  separationDistance: d.f32,
  separationStrength: d.f32,
  alignmentDistance: d.f32,
  alignmentStrength: d.f32,
  cohesionDistance: d.f32,
  cohesionStrength: d.f32,
});

const TriangleData = d.struct({
  position: d.vec2f,
  velocity: d.vec2f,
});

const TriangleDataArray = (n: number) => d.arrayOf(TriangleData, n);

const renderBindGroupLayout = tgpu.bindGroupLayout({
  trianglePos: { storage: TriangleDataArray },
  colorPalette: { uniform: d.vec3f },
});

const computeBindGroupLayout = tgpu.bindGroupLayout({
  currentTrianglePos: { storage: TriangleDataArray },
  nextTrianglePos: { storage: TriangleDataArray, access: "mutable" },
  params: { uniform: Parameters },
});

const colorPresets = {
  plumTree: d.vec3f(1.0, 2.0, 1.0),
  jeans: d.vec3f(2.0, 1.5, 1.0),
  greyscale: d.vec3f(0, 0, 0),
  hotcold: d.vec3f(0, 3.14, 3.14),
} as const;
type ColorPresets = keyof typeof colorPresets;

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

export function ComputeBoids() {
  const randomizePositions = useRef<() => void>(() => {});
  const updateParams = useRef<(newOptions: BoidsOptions) => void>(() => {});
  const updateColorPreset = useRef<(newColorPreset: ColorPresets) => void>(
    () => {},
  );

  const ref = useWebGPU(({ context, device, presentationFormat }) => {
    const root = tgpu.initFromDevice({ device });

    context.configure({
      device,
      format: presentationFormat,
      alphaMode: "premultiplied",
    });

    const paramsBuffer = root
      .createBuffer(Parameters, presets.default)
      .$usage("uniform");

    const triangleSize = 0.03;
    const triangleVertexBuffer = root
      .createBuffer(d.arrayOf(d.f32, 6), [
        0.0,
        triangleSize,
        -triangleSize / 2,
        -triangleSize / 2,
        triangleSize / 2,
        -triangleSize / 2,
      ])
      .$usage("vertex");

    const triangleAmount = 1000;
    const trianglePosBuffers = Array.from({ length: 2 }, () =>
      root.createBuffer(TriangleDataArray(triangleAmount)).$usage("storage"),
    );

    randomizePositions.current = () => {
      const positions = Array.from({ length: triangleAmount }, () => ({
        position: d.vec2f(Math.random() * 2 - 1, Math.random() * 2 - 1),
        velocity: d.vec2f(
          Math.random() * 0.1 - 0.05,
          Math.random() * 0.1 - 0.05,
        ),
      }));
      trianglePosBuffers[0].write(positions);
      trianglePosBuffers[1].write(positions);
    };
    randomizePositions.current();

    const colorPaletteBuffer = root
      .createBuffer(d.vec3f, colorPresets.plumTree)
      .$usage("uniform");

    updateColorPreset.current = (newColorPreset: ColorPresets) => {
      colorPaletteBuffer.write(colorPresets[newColorPreset]);
    };

    updateParams.current = (newOptions: BoidsOptions) => {
      paramsBuffer.write(newOptions);
    };

    const renderModule = device.createShaderModule({
      code: tgpu.resolve({
        template: renderCode,
        externals: {
          _EXT_: {
            ...renderBindGroupLayout.bound,
          },
        },
      }),
    });

    const computeModule = device.createShaderModule({
      code: tgpu.resolve({
        template: computeCode,
        externals: {
          _EXT_: {
            ...computeBindGroupLayout.bound,
          },
        },
      }),
    });

    const pipeline = device.createRenderPipeline({
      layout: device.createPipelineLayout({
        bindGroupLayouts: [root.unwrap(renderBindGroupLayout)],
      }),
      vertex: {
        module: renderModule,
        buffers: [
          {
            arrayStride: 2 * 4,
            attributes: [
              {
                shaderLocation: 0,
                offset: 0,
                format: "float32x2" as const,
              },
            ],
          },
        ],
      },
      fragment: {
        module: renderModule,
        targets: [
          {
            format: presentationFormat,
          },
        ],
      },
      primitive: {
        topology: "triangle-list",
      },
    });

    const computePipeline = device.createComputePipeline({
      layout: device.createPipelineLayout({
        bindGroupLayouts: [root.unwrap(computeBindGroupLayout)],
      }),
      compute: {
        module: computeModule,
      },
    });

    const renderBindGroups = [0, 1].map((idx) =>
      root.createBindGroup(renderBindGroupLayout, {
        trianglePos: trianglePosBuffers[idx],
        colorPalette: colorPaletteBuffer,
      }),
    );

    const computeBindGroups = [0, 1].map((idx) =>
      root.createBindGroup(computeBindGroupLayout, {
        currentTrianglePos: trianglePosBuffers[idx],
        nextTrianglePos: trianglePosBuffers[1 - idx],
        params: paramsBuffer,
      }),
    );

    const renderPassDescriptor: GPURenderPassDescriptor = {
      colorAttachments: [
        {
          view: toBeAssignedLater(),
          clearValue: [1, 1, 1, 1],
          loadOp: "clear" as const,
          storeOp: "store" as const,
        },
      ],
    };

    let even = false;
    function frame() {
      even = !even;
      (
        renderPassDescriptor.colorAttachments as [GPURenderPassColorAttachment]
      )[0].view = context.getCurrentTexture().createView();

      const commandEncoder = device.createCommandEncoder();
      const computePass = commandEncoder.beginComputePass();
      computePass.setPipeline(computePipeline);
      computePass.setBindGroup(
        0,
        root.unwrap(even ? computeBindGroups[0] : computeBindGroups[1]),
      );
      computePass.dispatchWorkgroups(triangleAmount);
      computePass.end();

      const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
      passEncoder.setPipeline(pipeline);
      passEncoder.setVertexBuffer(0, triangleVertexBuffer.buffer);
      passEncoder.setBindGroup(
        0,
        root.unwrap(even ? renderBindGroups[1] : renderBindGroups[0]),
      );
      passEncoder.draw(3, triangleAmount);
      passEncoder.end();

      device.queue.submit([commandEncoder.finish()]);
    }
    return frame;
  });

  const randomizeHandler = useCallback(() => {
    randomizePositions.current();
  }, []);

  const handleChoosePreset = useCallback(
    (params: BoidsOptions) => () => {
      updateParams.current(params);
    },
    [],
  );

  const colorPresetHandler = useCallback(
    (preset: ColorPresets) => () => {
      updateColorPreset.current(preset);
    },
    [],
  );

  return (
    <View style={style.container}>
      <Canvas ref={ref} style={style.webgpu} />
      <View style={style.controls}>
        <View style={style.buttonRow}>
          <Text style={style.spanText}>randomize: </Text>
          <Button title="🔀" onPress={randomizeHandler} />
        </View>
        <View style={style.buttonRow}>
          <Text style={style.spanText}>presets: </Text>
          <Button title="🐦" onPress={handleChoosePreset(presets.default)} />
          <Button title="🦟" onPress={handleChoosePreset(presets.mosquitos)} />
          <Button title="💧" onPress={handleChoosePreset(presets.blobs)} />
          <Button title="⚛️" onPress={handleChoosePreset(presets.particles)} />
        </View>
        <View style={style.buttonRow}>
          <Text style={style.spanText}>colors: </Text>
          <Button title="🟪🟩" onPress={colorPresetHandler("plumTree")} />
          <Button title="🟦🟫" onPress={colorPresetHandler("jeans")} />
          <Button title="⬛️⬜️" onPress={colorPresetHandler("greyscale")} />
          <Button title="🟥🟦" onPress={colorPresetHandler("hotcold")} />
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
