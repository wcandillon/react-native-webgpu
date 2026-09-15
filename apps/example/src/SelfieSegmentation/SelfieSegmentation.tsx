import React, { useMemo, useRef, useState } from "react";
import { StyleSheet, Text, TouchableOpacity, View } from "react-native";
import { createSynchronizable } from "react-native-worklets";
import * as tf from "@tensorflow/tfjs";
import * as bodySegmentation from "@tensorflow-models/body-segmentation";

import { ensureTfjsWebGPU } from "../VisionCamera/tfjs";
import { useCameraInference } from "../VisionCamera/useCameraInference";

import { SHADER } from "./shader";

// MediaPipe Selfie Segmentation's "general" model runs at 256x256 and hands
// back a mask at the input size, so the mask texture matches the model
// input one to one.
const INPUT_SIZE = 256;
const UNIFORM_SIZE = 32;

const MODES = [
  { id: 0, label: "Blur" },
  { id: 1, label: "Replace" },
  { id: 2, label: "Spotlight" },
];

interface PipelineState {
  pipeline: GPURenderPipeline;
  uniformBuffer: GPUBuffer;
  maskView: GPUTextureView;
  maskSampler: GPUSampler;
  startTime: number;
}

const loadSegmenter = async () => {
  await ensureTfjsWebGPU();
  return bodySegmentation.createSegmenter(
    bodySegmentation.SupportedModels.MediaPipeSelfieSegmentation,
    { runtime: "tfjs", modelType: "general" },
  );
};

export const SelfieSegmentation = () => {
  const [status, setStatus] = useState("Loading selfie segmentation...");
  const [mode, setMode] = useState(0);
  // The worklet reads the mode every frame; the buttons write it.
  const modeSync = useMemo(() => createSynchronizable(0), []);
  // Owned by the render device. The main thread uploads a fresh mask into
  // it after every inference with queue.writeTexture, the worklet samples
  // it. Queue ordering keeps the two consistent.
  const maskRef = useRef<{ device: GPUDevice; texture: GPUTexture } | null>(
    null,
  );

  const segmenterPromise = useMemo(() => {
    const p = loadSegmenter();
    p.then(() => setStatus("Segmenting...")).catch(() => {});
    return p;
  }, []);

  const { element, error } = useCameraInference<PipelineState>({
    inputSize: INPUT_SIZE,
    cameraPosition: "front",
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
      const maskTexture = device.createTexture({
        size: [INPUT_SIZE, INPUT_SIZE],
        format: "r8unorm",
        usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST,
      });
      const maskSampler = device.createSampler({
        magFilter: "linear",
        minFilter: "linear",
        addressModeU: "clamp-to-edge",
        addressModeV: "clamp-to-edge",
      });
      maskRef.current = { device, texture: maskTexture };
      return {
        pipeline,
        uniformBuffer,
        maskView: maskTexture.createView(),
        maskSampler,
        startTime: Date.now(),
      };
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
      const { pipeline, uniformBuffer, maskView, maskSampler, startTime } =
        pipelineState;

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
          { binding: 3, resource: maskView },
          { binding: 4, resource: maskSampler },
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
      const segmenter = await segmenterPromise;
      const mask = maskRef.current;
      if (!mask) {
        return;
      }
      const tensor = tf.tensor3d(rgb, [size, size, 3]);
      const segmentations = await segmenter
        .segmentPeople(tensor)
        .finally(() => tensor.dispose());
      if (segmentations.length === 0) {
        return;
      }
      // The tfjs runtime returns an [h, w, 4] float tensor with the person
      // probability in the first channel.
      const maskTensor = await segmentations[0].mask.toTensor();
      const [height, width] = maskTensor.shape;
      const channel = tf.slice(maskTensor, [0, 0, 0], [height, width, 1]);
      let values: Float32Array | Int32Array | Uint8Array;
      try {
        values = await channel.data();
      } finally {
        channel.dispose();
        maskTensor.dispose();
      }
      if (width !== size || height !== size) {
        console.warn(
          `[SelfieSegmentation] unexpected mask size ${width}x${height}`,
        );
        return;
      }
      const bytes = new Uint8Array(size * size);
      for (let i = 0; i < bytes.length; i++) {
        bytes[i] = Math.round(Math.min(1, Math.max(0, values[i])) * 255);
      }
      mask.device.queue.writeTexture(
        { texture: mask.texture },
        bytes,
        { bytesPerRow: size },
        [size, size],
      );
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
