import React, { useMemo, useState } from "react";
import { StyleSheet, Text, View } from "react-native";
import { createSynchronizable } from "react-native-worklets";
import * as tf from "@tensorflow/tfjs";

import { ensureTfjsWebGPU } from "../VisionCamera/tfjs";
import { useCameraInference } from "../VisionCamera/useCameraInference";

import { loadCocoSsd, type DetectedObject } from "./cocoSsd";
import { SHADER } from "./shader";

// COCO-SSD (lite MobileNet v2) resizes internally to 300x300, so a 320x320
// input keeps full detail while satisfying the readback row alignment.
const INPUT_SIZE = 320;
const MAX_BOXES = 10;
const MIN_SCORE = 0.45;
// 32-byte header + 32 bytes (rect + meta) per box, see shader.ts.
const UNIFORM_SIZE = 32 + 32 * MAX_BOXES;

// COCO class -> the kind index the shader colors by.
const KIND: Record<string, number> = { dog: 1, person: 2, cat: 3 };

interface PipelineState {
  pipeline: GPURenderPipeline;
  uniformBuffer: GPUBuffer;
  startTime: number;
}

// Written by the main thread, read by the camera worklet. Eight floats per
// box, matching the Box struct: x, y, w, h, kind, score, 0, 0.
interface Detections {
  count: number;
  data: number[];
}

const loadModel = async () => {
  await ensureTfjsWebGPU();
  return loadCocoSsd();
};

export const ObjectDetection = () => {
  const [status, setStatus] = useState("Loading COCO-SSD...");
  const [dogSeen, setDogSeen] = useState(false);

  const detections = useMemo(
    () =>
      createSynchronizable<Detections>({
        count: 0,
        data: new Array<number>(MAX_BOXES * 8).fill(0),
      }),
    [],
  );
  const modelPromise = useMemo(() => {
    const p = loadModel();
    p.then(() => setStatus("Looking for objects...")).catch(() => {});
    return p;
  }, []);

  const { element, error } = useCameraInference<PipelineState>({
    inputSize: INPUT_SIZE,
    cameraPosition: "back",
    setup: ({ device, presentationFormat }) => {
      const module = device.createShaderModule({ code: SHADER });
      const pipeline = device.createRenderPipeline({
        layout: "auto",
        vertex: { module, entryPoint: "vs_main" },
        fragment: {
          module,
          entryPoint: "fs_main",
          targets: [{ format: presentationFormat }],
        },
        primitive: { topology: "triangle-list" },
      });
      const uniformBuffer = device.createBuffer({
        size: UNIFORM_SIZE,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
      });
      return { pipeline, uniformBuffer, startTime: Date.now() };
    },
    render: ({
      device,
      context,
      externalTexture,
      canvasWidth,
      canvasHeight,
      frameWidth,
      frameHeight,
      rotation,
      mirror,
      sampler,
      pipelineState,
    }) => {
      "worklet";
      const { pipeline, uniformBuffer, startTime } = pipelineState;
      const found = detections.getDirty();

      const uniformData = new ArrayBuffer(UNIFORM_SIZE);
      const uniformF32 = new Float32Array(uniformData);
      const uniformU32 = new Uint32Array(uniformData);
      uniformF32[0] = frameWidth;
      uniformF32[1] = frameHeight;
      uniformF32[2] = canvasWidth;
      uniformF32[3] = canvasHeight;
      uniformU32[4] = found.count;
      uniformF32[5] = (Date.now() - startTime) / 1000;
      uniformU32[6] = rotation;
      uniformU32[7] = mirror;
      uniformF32.set(found.data, 8);
      device.queue.writeBuffer(uniformBuffer, 0, uniformData);

      const bindGroup = device.createBindGroup({
        layout: pipeline.getBindGroupLayout(0),
        entries: [
          { binding: 0, resource: externalTexture },
          { binding: 1, resource: sampler },
          { binding: 2, resource: { buffer: uniformBuffer } },
        ],
      });
      const encoder = device.createCommandEncoder();
      const pass = encoder.beginRenderPass({
        colorAttachments: [
          {
            view: context.getCurrentTexture().createView(),
            clearValue: { r: 0, g: 0, b: 0, a: 1 },
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      });
      pass.setPipeline(pipeline);
      pass.setBindGroup(0, bindGroup);
      pass.draw(3);
      pass.end();
      device.queue.submit([encoder.finish()]);
      context.present();
    },
    inference: async (rgb, size) => {
      const model = await modelPromise;
      // The SSD graph takes a uint8 image tensor, so build it as int32.
      const tensor = tf.tensor3d(rgb, [size, size, 3], "int32");
      let found: DetectedObject[];
      try {
        found = await model.detect(tensor, MAX_BOXES, MIN_SCORE);
      } finally {
        tensor.dispose();
      }
      const count = Math.min(found.length, MAX_BOXES);
      const data = new Array<number>(MAX_BOXES * 8).fill(0);
      for (let i = 0; i < count; i++) {
        const [x, y, w, h] = found[i].bbox;
        data[i * 8 + 0] = x / size;
        data[i * 8 + 1] = y / size;
        data[i * 8 + 2] = w / size;
        data[i * 8 + 3] = h / size;
        data[i * 8 + 4] = KIND[found[i].class] ?? 0;
        data[i * 8 + 5] = found[i].score;
      }
      detections.setBlocking({ count, data });

      setDogSeen(found.some((d) => d.class === "dog"));
      if (count > 0) {
        console.log(
          "[ObjectDetection] " +
            found
              .slice(0, count)
              .map((d) => `${d.class} ${Math.round(d.score * 100)}%`)
              .join(", "),
        );
      }
    },
  });

  const headline = error ?? (dogSeen ? "🐶 Dog detected!" : status);

  return (
    <View style={styles.root}>
      {element}
      <View style={styles.statusBar}>
        <Text style={error ? styles.errorText : styles.statusText}>
          {headline}
        </Text>
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: "black" },
  statusBar: {
    position: "absolute",
    top: 16,
    left: 16,
    right: 16,
    backgroundColor: "rgba(0,0,0,0.55)",
    paddingHorizontal: 10,
    paddingVertical: 6,
    borderRadius: 6,
  },
  statusText: { color: "white", fontSize: 12 },
  errorText: { color: "#ff6b6b", fontSize: 12 },
});
