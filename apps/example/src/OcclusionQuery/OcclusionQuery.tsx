/* eslint-disable @typescript-eslint/ban-ts-comment */
import React, { useState } from "react";
import { StyleSheet, View, Text } from "react-native";
import { Canvas } from "react-native-wgpu";
import { mat4 } from "wgpu-matrix";

import { useWebGPU } from "../components/useWebGPU";

const solidColorLitWGSL = /*wgsl*/ `struct Uniforms {
  worldViewProjectionMatrix: mat4x4f,
  worldMatrix: mat4x4f,
  color: vec4f,
};

struct Vertex {
  @location(0) position: vec4f,
  @location(1) normal: vec3f,
};

struct VSOut {
  @builtin(position) position: vec4f,
  @location(0) normal: vec3f,
};

@group(0) @binding(0) var<uniform> uni: Uniforms;

@vertex fn vs(vin: Vertex) -> VSOut {
  var vOut: VSOut;
  vOut.position = uni.worldViewProjectionMatrix * vin.position;
  vOut.normal = (uni.worldMatrix * vec4f(vin.normal, 0)).xyz;
  return vOut;
}

@fragment fn fs(vin: VSOut) -> @location(0) vec4f {
  let lightDirection = normalize(vec3f(4, 10, 6));
  let light = dot(normalize(vin.normal), lightDirection) * 0.5 + 0.5;
  return vec4f(uni.color.rgb * light, uni.color.a);
}
`;

type TypedArrayView =
  | Int8Array
  | Uint8Array
  | Int16Array
  | Uint16Array
  | Int32Array
  | Uint32Array
  | Float32Array
  | Float64Array;

export type TypedArrayConstructor =
  | Int8ArrayConstructor
  | Uint8ArrayConstructor
  | Int16ArrayConstructor
  | Uint16ArrayConstructor
  | Int32ArrayConstructor
  | Uint32ArrayConstructor
  | Float32ArrayConstructor
  | Float64ArrayConstructor;

export function OcclusionQuery() {
  const [visible, setVisible] = useState("");
  const ref = useWebGPU(({ context, device, presentationFormat, canvas }) => {
    context.configure({
      device,
      format: presentationFormat,
      alphaMode: "premultiplied",
    });
    const depthFormat = "depth24plus";
    const animate = true;
    const module = device.createShaderModule({
      code: solidColorLitWGSL,
    });

    const pipeline = device.createRenderPipeline({
      layout: "auto",
      vertex: {
        module,
        buffers: [
          {
            arrayStride: 6 * 4, // 3x2 floats, 4 bytes each
            attributes: [
              { shaderLocation: 0, offset: 0, format: "float32x3" }, // position
              { shaderLocation: 1, offset: 12, format: "float32x3" }, // normal
            ],
          },
        ],
      },
      fragment: {
        module,
        targets: [{ format: presentationFormat }],
      },
      primitive: {
        topology: "triangle-list",
        cullMode: "back",
      },
      depthStencil: {
        depthWriteEnabled: true,
        depthCompare: "less",
        format: depthFormat,
      },
    });

    // prettier-ignore
    const cubePositions = [
{ position: [-1,  0,  0], id: "🟥", color: [1, 0, 0, 1] },
{ position: [ 1,  0,  0], id: "🟨", color: [1, 1, 0, 1] },
{ position: [ 0, -1,  0], id: "🟩", color: [0, 0.5, 0, 1] },
{ position: [ 0,  1,  0], id: "🟧", color: [1, 0.6, 0, 1] },
{ position: [ 0,  0, -1], id: "🟦", color: [0, 0, 1, 1] },
{ position: [ 0,  0,  1], id: "🟪", color: [0.5, 0, 0.5, 1] },
];

    const objectInfos = cubePositions.map(({ position, id, color }) => {
      const uniformBufferSize = (2 * 16 + 3 + 1 + 4) * 4;
      const uniformBuffer = device.createBuffer({
        size: uniformBufferSize,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
      });
      const uniformValues = new Float32Array(uniformBufferSize / 4);
      const worldViewProjection = uniformValues.subarray(0, 16);
      const worldInverseTranspose = uniformValues.subarray(16, 32);
      const colorValue = uniformValues.subarray(32, 36);

      colorValue.set(color);

      const bindGroup = device.createBindGroup({
        layout: pipeline.getBindGroupLayout(0),
        entries: [{ binding: 0, resource: { buffer: uniformBuffer } }],
      });

      return {
        id,
        position: position.map((v) => v * 10),
        bindGroup,
        uniformBuffer,
        uniformValues,
        worldInverseTranspose,
        worldViewProjection,
      };
    });

    const querySet = device.createQuerySet({
      type: "occlusion",
      count: objectInfos.length,
    });

    const resolveBuf = device.createBuffer({
      label: "resolveBuffer",
      // Query results are 64bit unsigned integers.
      size: objectInfos.length * BigUint64Array.BYTES_PER_ELEMENT,
      usage: GPUBufferUsage.QUERY_RESOLVE | GPUBufferUsage.COPY_SRC,
    });

    const resultBuf = device.createBuffer({
      label: "resultBuffer",
      size: resolveBuf.size,
      usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
    });

    function createBufferWithData(
      dev: GPUDevice,
      data: TypedArrayView,
      usage: GPUBufferUsageFlags,
      label: string,
    ) {
      const buffer = dev.createBuffer({
        label,
        size: data.byteLength,
        usage,
        mappedAtCreation: true,
      });
      const Ctor = data.constructor as TypedArrayConstructor;
      const dst = new Ctor(buffer.getMappedRange());
      dst.set(data);
      buffer.unmap();
      return buffer;
    }

    // prettier-ignore
    const vertexData = new Float32Array([
// position       normal
 1,  1, -1,     1,  0,  0,
 1,  1,  1,     1,  0,  0,
 1, -1,  1,     1,  0,  0,
 1, -1, -1,     1,  0,  0,
-1,  1,  1,    -1,  0,  0,
-1,  1, -1,    -1,  0,  0,
-1, -1, -1,    -1,  0,  0,
-1, -1,  1,    -1,  0,  0,
-1,  1,  1,     0,  1,  0,
 1,  1,  1,     0,  1,  0,
 1,  1, -1,     0,  1,  0,
-1,  1, -1,     0,  1,  0,
-1, -1, -1,     0, -1,  0,
 1, -1, -1,     0, -1,  0,
 1, -1,  1,     0, -1,  0,
-1, -1,  1,     0, -1,  0,
 1,  1,  1,     0,  0,  1,
-1,  1,  1,     0,  0,  1,
-1, -1,  1,     0,  0,  1,
 1, -1,  1,     0,  0,  1,
-1,  1, -1,     0,  0, -1,
 1,  1, -1,     0,  0, -1,
 1, -1, -1,     0,  0, -1,
-1, -1, -1,     0,  0, -1,
]);
    // prettier-ignore
    const indices = new Uint16Array([
 0,  1,  2,  0,  2,  3, // +x face
 4,  5,  6,  4,  6,  7, // -x face
 8,  9, 10,  8, 10, 11, // +y face
12, 13, 14, 12, 14, 15, // -y face
16, 17, 18, 16, 18, 19, // +z face
20, 21, 22, 20, 22, 23, // -z face
]);

    const vertexBuf = createBufferWithData(
      device,
      vertexData,
      GPUBufferUsage.VERTEX,
      "vertexBuffer",
    );
    const indicesBuf = createBufferWithData(
      device,
      indices,
      GPUBufferUsage.INDEX,
      "indexBuffer",
    );

    const renderPassDescriptor: GPURenderPassDescriptor = {
      // @ts-expect-error
      colorAttachments: [
        {
          view: undefined, // Assigned later
          clearValue: { r: 0.5, g: 0.5, b: 0.5, a: 1.0 },
          loadOp: "clear",
          storeOp: "store",
        },
      ],
      depthStencilAttachment: {
        // @ts-expect-error
        view: undefined, // Assigned later
        depthClearValue: 1.0,
        depthLoadOp: "clear",
        depthStoreOp: "store",
      },
      occlusionQuerySet: querySet,
    };

    const lerp = (a: number, b: number, t: number) => a + (b - a) * t;
    const lerpV = (a: number[], b: number[], t: number) =>
      a.map((v, i) => lerp(v, b[i], t));
    const pingPongSine = (t: number) => Math.sin(t * Math.PI * 2) * 0.5 + 0.5;

    let depthTexture: GPUTexture | undefined;

    let time = 0;
    let then = 0;
    //let visible = "";
    function render(now: number) {
      now *= 0.001; // convert to seconds
      const deltaTime = now - then;
      then = now;

      if (animate) {
        time += deltaTime;
      }

      const projection = mat4.perspective(
        (30 * Math.PI) / 180,
        canvas.width / canvas.height,
        0.5,
        100,
      );

      const m = mat4.identity();
      mat4.rotateX(m, time, m);
      mat4.rotateY(m, time * 0.7, m);
      mat4.translate(
        m,
        lerpV([0, 0, 5], [0, 0, 40], pingPongSine(time * 0.2)),
        m,
      );
      const view = mat4.inverse(m);
      const viewProjection = mat4.multiply(projection, view);

      const canvasTexture = context.getCurrentTexture();
      if (
        !depthTexture ||
        depthTexture.width !== canvasTexture.width ||
        depthTexture.height !== canvasTexture.height
      ) {
        depthTexture?.destroy();
        depthTexture = device.createTexture({
          size: canvasTexture, // canvasTexture has width, height, and depthOrArrayLayers properties
          format: depthFormat,
          usage: GPUTextureUsage.RENDER_ATTACHMENT,
        });
      }

      const colorTexture = context.getCurrentTexture();
      //@ts-expect-error
      renderPassDescriptor.colorAttachments[0].view = colorTexture.createView();
      //@ts-expect-error
      renderPassDescriptor.depthStencilAttachment.view =
        depthTexture.createView();

      const encoder = device.createCommandEncoder();
      const pass = encoder.beginRenderPass(renderPassDescriptor);
      pass.setPipeline(pipeline);
      pass.setVertexBuffer(0, vertexBuf);
      pass.setIndexBuffer(indicesBuf, "uint16");

      objectInfos.forEach(
        (
          {
            bindGroup,
            uniformBuffer,
            uniformValues,
            worldViewProjection,
            worldInverseTranspose,
            position,
          },
          i,
        ) => {
          const world = mat4.translation(position);
          mat4.transpose(mat4.inverse(world), worldInverseTranspose);
          mat4.multiply(viewProjection, world, worldViewProjection);

          device.queue.writeBuffer(uniformBuffer, 0, uniformValues);

          pass.setBindGroup(0, bindGroup);
          pass.beginOcclusionQuery(i);
          pass.drawIndexed(indices.length);
          pass.endOcclusionQuery();
        },
      );

      pass.end();
      encoder.resolveQuerySet(querySet, 0, objectInfos.length, resolveBuf, 0);
      if (resultBuf.mapState === "unmapped") {
        encoder.copyBufferToBuffer(resolveBuf, 0, resultBuf, 0, resultBuf.size);
      }

      device.queue.submit([encoder.finish()]);

      if (resultBuf.mapState === "unmapped") {
        resultBuf.mapAsync(GPUMapMode.READ).then(() => {
          const results = new BigUint64Array(resultBuf.getMappedRange());

          const v = objectInfos
            .filter((_, i) => results[i])
            .map(({ id }) => id)
            .join("");
          setVisible(v);
          // console.log({ visible });
          resultBuf.unmap();
        });
      }
    }
    return render;
  });

  return (
    <View style={style.container}>
      <View style={{ height: 32, justifyContent: "center" }}>
        <Text>{visible}</Text>
      </View>
      <Canvas ref={ref} style={style.webgpu} />
    </View>
  );
}

const style = StyleSheet.create({
  container: {
    flex: 1,
  },
  webgpu: {
    flex: 1,
  },
});
