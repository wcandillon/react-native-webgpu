import React, { useEffect, useState } from "react";
import tgpu from "typegpu";
import { arrayOf, f32, struct } from "typegpu/data";
import { StyleSheet, View, Text, Button } from "react-native";
import { Canvas } from "react-native-wgpu";

import { useWebGPU } from "../../components/useWebGPU";
import { toBeAssignedLater } from "../../components/utils";

import { renderCode, computeCode } from "./Shaders";

let randomizePositions: () => void = () => {};
let updateParams: (newOptions: Options) => void = () => {};
type Options = {
  separationDistance: number;
  separationStrength: number;
  alignmentDistance: number;
  alignmentStrength: number;
  cohesionDistance: number;
  cohesionStrength: number;
};

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
};

export function Boids() {
  const [randomize, setRandomize] = useState(true);
  const [options, setOptions] = useState<Options>({
    separationDistance: 0.05,
    separationStrength: 0.001,
    alignmentDistance: 0.3,
    alignmentStrength: 0.01,
    cohesionDistance: 0.3,
    cohesionStrength: 0.001,
  });

  useEffect(() => {
    if (randomize) {
      console.log("Randomizing positions.");
      randomizePositions();
      setRandomize(false);
    }
  }, [randomize]);

  useEffect(() => {
    if (options) {
      console.log("Updating params.", options);
      updateParams(options);
    }
  }, [options]);

  const { canvasRef } = useWebGPU(({ context, device, presentationFormat }) => {
    context.configure({
      device,
      format: presentationFormat,
      alphaMode: "premultiplied",
    });

    const params = struct({
      separationDistance: f32,
      separationStrength: f32,
      alignmentDistance: f32,
      alignmentStrength: f32,
      cohesionDistance: f32,
      cohesionStrength: f32,
    });

    const paramsBuffer = tgpu
      .createBuffer(params)
      .$device(device)
      .$usage(tgpu.Storage);

    const triangleSize = 0.03;
    const triangleVertexBuffer = tgpu
      .createBuffer(arrayOf(f32, 6), [
        0.0,
        triangleSize,
        -triangleSize / 2,
        -triangleSize / 2,
        triangleSize / 2,
        -triangleSize / 2,
      ])
      .$device(device)
      .$usage(tgpu.Vertex);

    const triangleAmount = 1000;
    const triangleInfoStruct = struct({
      positionX: f32,
      positionY: f32,
      velocityX: f32,
      velocityY: f32,
    });
    const trianglePosBuffers = Array.from({ length: 2 }, () =>
      tgpu
        .createBuffer(arrayOf(triangleInfoStruct, triangleAmount))
        .$device(device)
        .$usage(tgpu.Storage, tgpu.Uniform),
    );

    randomizePositions = () => {
      const positions = Array.from({ length: triangleAmount }, () => ({
        positionX: Math.random() * 2 - 1,
        positionY: Math.random() * 2 - 1,
        velocityX: Math.random() * 0.1 - 0.05,
        velocityY: Math.random() * 0.1 - 0.05,
      }));
      tgpu.write(trianglePosBuffers[0], positions);
      tgpu.write(trianglePosBuffers[1], positions);
    };
    randomizePositions();

    updateParams = (newOptions: Options) => {
      tgpu.write(paramsBuffer, newOptions);
    };
    updateParams(presets.default);

    const renderModule = device.createShaderModule({
      code: renderCode,
    });

    const computeModule = device.createShaderModule({
      code: computeCode,
    });

    const pipeline = device.createRenderPipeline({
      layout: "auto",
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
      layout: "auto",
      compute: {
        module: computeModule,
      },
    });

    const renderBindGroups = [0, 1].map((idx) =>
      device.createBindGroup({
        layout: pipeline.getBindGroupLayout(0),
        entries: [
          {
            binding: 0,
            resource: {
              buffer: trianglePosBuffers[idx].buffer,
            },
          },
        ],
      }),
    );

    const computeBindGroups = [0, 1].map((idx) =>
      device.createBindGroup({
        layout: computePipeline.getBindGroupLayout(0),
        entries: [
          {
            binding: 0,
            resource: {
              buffer: trianglePosBuffers[idx].buffer,
            },
          },
          {
            binding: 1,
            resource: {
              buffer: trianglePosBuffers[1 - idx].buffer,
            },
          },
          {
            binding: 2,
            resource: {
              buffer: paramsBuffer.buffer,
            },
          },
        ],
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
      renderPassDescriptor.colorAttachments[0].view = context
        .getCurrentTexture()
        .createView();

      const commandEncoder = device.createCommandEncoder();
      const computePass = commandEncoder.beginComputePass();
      computePass.setPipeline(computePipeline);
      computePass.setBindGroup(
        0,
        even ? computeBindGroups[0] : computeBindGroups[1],
      );
      computePass.dispatchWorkgroups(triangleAmount);
      computePass.end();

      const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
      passEncoder.setPipeline(pipeline);
      passEncoder.setVertexBuffer(0, triangleVertexBuffer.buffer);
      passEncoder.setBindGroup(
        0,
        even ? renderBindGroups[1] : renderBindGroups[0],
      );
      passEncoder.draw(3, triangleAmount);
      passEncoder.end();

      device.queue.submit([commandEncoder.finish()]);
    }
    return frame;
  });

  return (
    <View style={style.container}>
      <Canvas ref={canvasRef} style={style.webgpu} />
      <View style={style.controls}>
        <View style={style.buttonRow}>
          <Text style={style.spanText}>randomize: </Text>
          <Button title="🔀" onPress={() => setRandomize(!randomize)} />
        </View>
        <View style={style.buttonRow}>
          <Text style={style.spanText}>presets: </Text>
          <Button title="🐦" onPress={() => setOptions(presets.default)} />
          <Button title="🦟" onPress={() => setOptions(presets.mosquitos)} />
          <Button title="💧" onPress={() => setOptions(presets.blobs)} />
          <Button title="⚛️" onPress={() => setOptions(presets.particles)} />
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
