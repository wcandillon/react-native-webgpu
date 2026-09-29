import React, { useMemo, useRef, useState } from "react";
import { StyleSheet, Text, TouchableOpacity, View } from "react-native";
import { createSynchronizable } from "react-native-worklets";
import * as bodySegmentation from "@tensorflow-models/body-segmentation";

import { useCameraInference } from "../VisionCamera/useCameraInference";

import { makeShader } from "./shader";

// MediaPipe Selfie Segmentation's "general" model runs at 256x256 and hands
// back a mask at the input size, so the mask buffer matches the model input
// one to one. The whole pipeline stays on the GPU: camera frame, model,
// mask, and the WGSL compositing all share one device.
const INPUT_SIZE = 256;
const UNIFORM_SIZE = 32;
// [h, w, 4] float32, the layout the tfjs segmenter produces.
const MASK_BYTES = INPUT_SIZE * INPUT_SIZE * 4 * 4;

const MODES = [
  { id: 0, label: "Blur" },
  { id: 1, label: "Replace" },
  { id: 2, label: "Spotlight" },
];

interface PipelineState {
  pipeline: GPURenderPipeline;
  uniformBuffer: GPUBuffer;
  maskBuffer: GPUBuffer;
  startTime: number;
}

type Segmenter = bodySegmentation.BodySegmenter;

const loadSegmenter = () =>
  bodySegmentation.createSegmenter(
    bodySegmentation.SupportedModels.MediaPipeSelfieSegmentation,
    { runtime: "tfjs", modelType: "general" },
  );

export const SelfieSegmentation = () => {
  const [status, setStatus] = useState("Loading selfie segmentation...");
  const [mode, setMode] = useState(0);
  // The worklet reads the mode every frame; the buttons write it.
  const modeSync = useMemo(() => createSynchronizable(0), []);
  // Model output lands in this buffer through a GPU copy after every
  // inference; the worklet's shader reads it. Queue ordering keeps the two
  // consistent.
  const maskRef = useRef<{ device: GPUDevice; buffer: GPUBuffer } | null>(null);
  const segmenterRef = useRef<Promise<Segmenter> | null>(null);

  const { element, error } = useCameraInference<PipelineState>({
    inputSize: INPUT_SIZE,
    cameraPosition: "front",
    setup: ({ device, presentationFormat }) => {
      // tfjs is bound to this device by now, so the model can load.
      const segmenter = loadSegmenter();
      segmenter.then(() => setStatus("Segmenting...")).catch(() => {});
      segmenterRef.current = segmenter;

      const module = device.createShaderModule({
        code: makeShader(INPUT_SIZE),
      });
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
      const maskBuffer = device.createBuffer({
        size: MASK_BYTES,
        usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST,
      });
      maskRef.current = { device, buffer: maskBuffer };
      return { pipeline, uniformBuffer, maskBuffer, startTime: Date.now() };
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
      const { pipeline, uniformBuffer, maskBuffer, startTime } = pipelineState;

      const uniformData = new ArrayBuffer(UNIFORM_SIZE);
      const uniformF32 = new Float32Array(uniformData);
      const uniformU32 = new Uint32Array(uniformData);
      uniformF32[0] = frameWidth;
      uniformF32[1] = frameHeight;
      uniformF32[2] = canvasWidth;
      uniformF32[3] = canvasHeight;
      uniformU32[4] = modeSync.getDirty();
      uniformF32[5] = (Date.now() - startTime) / 1000;
      uniformU32[6] = rotation;
      uniformU32[7] = mirror;
      device.queue.writeBuffer(uniformBuffer, 0, uniformData);

      const bindGroup = device.createBindGroup({
        layout: pipeline.getBindGroupLayout(0),
        entries: [
          { binding: 0, resource: externalTexture },
          { binding: 1, resource: sampler },
          { binding: 2, resource: { buffer: uniformBuffer } },
          { binding: 3, resource: { buffer: maskBuffer } },
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
      const segmenter = await segmenterRef.current;
      const mask = maskRef.current;
      if (!segmenter || !mask) {
        return;
      }
      const segmentations = await segmenter.segmentPeople(input);
      if (segmentations.length === 0) {
        return;
      }
      // The tfjs runtime returns an [h, w, 4] float tensor with the person
      // probability in the first channel. dataToGPU hands back the tensor's
      // buffer, which is copied into ours on the queue: no readback.
      const maskTensor = await segmentations[0].mask.toTensor();
      try {
        const [height, width] = maskTensor.shape;
        if (width !== size || height !== size) {
          console.warn(
            `[SelfieSegmentation] unexpected mask size ${width}x${height}`,
          );
          return;
        }
        const gpuData = maskTensor.dataToGPU();
        try {
          // The type is shared with the WebGL variant, hence the optional.
          const source = gpuData.buffer;
          if (!source) {
            throw new Error("mask tensor is not on the GPU");
          }
          const encoder = mask.device.createCommandEncoder();
          encoder.copyBufferToBuffer(
            source,
            0,
            mask.buffer,
            0,
            Math.min(source.size, MASK_BYTES),
          );
          mask.device.queue.submit([encoder.finish()]);
        } finally {
          gpuData.tensorRef.dispose();
        }
      } finally {
        maskTensor.dispose();
      }
    },
  });

  const selectMode = (id: number) => {
    setMode(id);
    modeSync.setBlocking(id);
  };

  return (
    <View style={styles.root}>
      {element}
      <View style={styles.statusBar}>
        <Text style={error ? styles.errorText : styles.statusText}>
          {error ?? status}
        </Text>
      </View>
      <View style={styles.toolbar}>
        {MODES.map((m) => (
          <TouchableOpacity
            key={m.id}
            onPress={() => selectMode(m.id)}
            style={[styles.button, mode === m.id && styles.buttonActive]}
          >
            <Text
              style={[
                styles.buttonText,
                mode === m.id && styles.buttonTextActive,
              ]}
            >
              {m.label}
            </Text>
          </TouchableOpacity>
        ))}
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
  toolbar: {
    position: "absolute",
    bottom: 32,
    left: 16,
    right: 16,
    flexDirection: "row",
    justifyContent: "center",
    gap: 10,
  },
  button: {
    backgroundColor: "rgba(0,0,0,0.55)",
    paddingHorizontal: 18,
    paddingVertical: 10,
    borderRadius: 20,
    borderWidth: 1,
    borderColor: "rgba(255,255,255,0.35)",
  },
  buttonActive: {
    backgroundColor: "rgba(255,255,255,0.9)",
    borderColor: "white",
  },
  buttonText: { color: "white", fontSize: 14, fontWeight: "600" },
  buttonTextActive: { color: "black" },
});
