import React, { useMemo, useRef, useState } from "react";
import { StyleSheet, Text, View } from "react-native";
import { createSynchronizable } from "react-native-worklets";
import * as faceDetection from "@tensorflow-models/face-detection";

import { useCameraInference } from "../VisionCamera/useCameraInference";

import { SHADER } from "./shader";

// BlazeFace's "short" variant accepts a 128x128 input but we feed it at
// 192x192 to keep some headroom for the bounding box regression.
const DETECT_SIZE = 192;
const MAX_FACES = 8;
const UNIFORM_SIZE = 32 + 16 * MAX_FACES;

interface PipelineState {
  pipeline: GPURenderPipeline;
  uniformBuffer: GPUBuffer;
  startTime: number;
}

// Face boxes in normalized upright-image UV space, packed as
// (xMin, yMin, width, height) per face. Written by the main thread, read by
// the camera worklet.
interface FaceBoxes {
  count: number;
  boxes: number[];
}

const loadDetector = async (setStatus: (s: string) => void) => {
  setStatus("Loading face detector model...");
  const detector = await faceDetection.createDetector(
    faceDetection.SupportedModels.MediaPipeFaceDetector,
    { runtime: "tfjs", modelType: "short" },
  );
  setStatus("Detecting faces...");
  return detector;
};

export const FaceDetection = () => {
  const [status, setStatus] = useState("Waiting for camera...");

  const faces = useMemo(
    () =>
      createSynchronizable<FaceBoxes>({
        count: 0,
        boxes: new Array<number>(MAX_FACES * 4).fill(0),
      }),
    [],
  );
  // Loading starts in setup, once tfjs is bound to the camera device.
  // Failures surface through the inference loop, which awaits this
  // promise; the extra catch only silences the unhandled rejection warning.
  const detectorRef = useRef<Promise<faceDetection.FaceDetector> | null>(null);

  const { element, error } = useCameraInference<PipelineState>({
    inputSize: DETECT_SIZE,
    cameraPosition: "front",
    setup: ({ device, presentationFormat }) => {
      const detector = loadDetector(setStatus);
      detector.catch(() => {});
      detectorRef.current = detector;

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
      const detected = faces.getDirty();

      const uniformData = new ArrayBuffer(UNIFORM_SIZE);
      const uniformF32 = new Float32Array(uniformData);
      const uniformU32 = new Uint32Array(uniformData);
      uniformF32[0] = frameWidth;
      uniformF32[1] = frameHeight;
      uniformF32[2] = canvasWidth;
      uniformF32[3] = canvasHeight;
      uniformU32[4] = detected.count;
      uniformF32[5] = (Date.now() - startTime) / 1000;
      uniformU32[6] = rotation;
      uniformU32[7] = mirror;
      // The faces array starts at byte 32 (index 8 in the F32 view).
      uniformF32.set(detected.boxes, 8);
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
    inference: async (input, size) => {
      const detector = await detectorRef.current;
      if (!detector) {
        return;
      }
      const detected = await detector.estimateFaces(input, {
        flipHorizontal: false,
      });
      const boxes = new Array<number>(MAX_FACES * 4).fill(0);
      const count = Math.min(detected.length, MAX_FACES);
      for (let i = 0; i < count; i++) {
        const b = detected[i].box;
        boxes[i * 4 + 0] = b.xMin / size;
        boxes[i * 4 + 1] = b.yMin / size;
        boxes[i * 4 + 2] = b.width / size;
        boxes[i * 4 + 3] = b.height / size;
      }
      faces.setBlocking({ count, boxes });
    },
  });

  return (
    <View style={styles.root}>
      {element}
      <View style={styles.statusBar}>
        <Text style={error ? styles.errorText : styles.statusText}>
          {error ?? status}
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
