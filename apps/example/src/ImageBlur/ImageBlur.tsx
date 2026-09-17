import React, { useEffect, useRef, useState } from "react";
import { PixelRatio, Pressable, StyleSheet, Text, View } from "react-native";
import { Canvas, type CanvasRef, useDevice } from "react-native-webgpu";

import { decodeImage } from "../components/useAssets";

import { createBlurPassPlan } from "./blurPlan";
import { BLUR_SHADER, FULLSCREEN_TEXTURE_SHADER } from "./Shaders";

const FILTER_SIZES = [5, 15, 31] as const;
const ITERATIONS = [1, 2, 4] as const;

const nextOption = <T,>(options: readonly T[], current: T) => {
  const index = options.indexOf(current);
  return options[(index + 1) % options.length];
};

export const ImageBlur = () => {
  const { device } = useDevice();
  const canvasRef = useRef<CanvasRef>(null);
  const [filterSize, setFilterSize] = useState<number>(15);
  const [iterations, setIterations] = useState<number>(2);
  const settingsRef = useRef({ filterSize, iterations });
  const renderRef = useRef<(() => void) | null>(null);
  settingsRef.current = { filterSize, iterations };

  useEffect(() => {
    if (!device) {
      return;
    }

    let cancelled = false;
    let render: (() => void) | null = null;
    const buffers: GPUBuffer[] = [];
    const textures: GPUTexture[] = [];

    const trackBuffer = (buffer: GPUBuffer) => {
      buffers.push(buffer);
      return buffer;
    };
    const trackTexture = (texture: GPUTexture) => {
      textures.push(texture);
      return texture;
    };
    const createFlipBuffer = (value: number) => {
      const buffer = trackBuffer(
        device.createBuffer({
          size: 4,
          mappedAtCreation: true,
          usage: GPUBufferUsage.UNIFORM,
        }),
      );
      new Uint32Array(buffer.getMappedRange())[0] = value;
      buffer.unmap();
      return buffer;
    };

    const start = async () => {
      const context = canvasRef.current?.getContext("webgpu");
      if (!context) {
        return;
      }

      const image = await decodeImage(require("../assets/Di-3d.png"));
      if (cancelled) {
        return;
      }

      const canvas = context.canvas as HTMLCanvasElement;
      const pixelRatio = PixelRatio.get();
      canvas.width = canvas.clientWidth * pixelRatio;
      canvas.height = canvas.clientHeight * pixelRatio;

      const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
      context.configure({
        device,
        format: presentationFormat,
        alphaMode: "premultiplied",
      });

      const blurModule = device.createShaderModule({ code: BLUR_SHADER });
      const blurPipeline = device.createComputePipeline({
        layout: "auto",
        compute: { module: blurModule },
      });
      const fullscreenModule = device.createShaderModule({
        code: FULLSCREEN_TEXTURE_SHADER,
      });
      const fullscreenPipeline = device.createRenderPipeline({
        layout: "auto",
        vertex: { module: fullscreenModule, entryPoint: "vertexMain" },
        fragment: {
          module: fullscreenModule,
          entryPoint: "fragmentMain",
          targets: [{ format: presentationFormat }],
        },
        primitive: { topology: "triangle-list" },
      });

      const sampler = device.createSampler({
        magFilter: "linear",
        minFilter: "linear",
      });
      const sourceTexture = trackTexture(
        device.createTexture({
          size: [image.width, image.height, 1],
          format: "rgba8unorm",
          usage:
            GPUTextureUsage.TEXTURE_BINDING |
            GPUTextureUsage.COPY_DST |
            GPUTextureUsage.RENDER_ATTACHMENT,
        }),
      );
      device.queue.copyExternalImageToTexture(
        { source: image },
        { texture: sourceTexture },
        [image.width, image.height],
      );

      const intermediateTextures = [0, 1].map(() =>
        trackTexture(
          device.createTexture({
            size: [image.width, image.height],
            format: "rgba8unorm",
            usage:
              GPUTextureUsage.COPY_DST |
              GPUTextureUsage.STORAGE_BINDING |
              GPUTextureUsage.TEXTURE_BINDING,
          }),
        ),
      );
      const horizontalFlip = createFlipBuffer(0);
      const verticalFlip = createFlipBuffer(1);
      const blurParamsBuffer = trackBuffer(
        device.createBuffer({
          size: 8,
          usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.UNIFORM,
        }),
      );

      const computeConstants = device.createBindGroup({
        layout: blurPipeline.getBindGroupLayout(0),
        entries: [
          { binding: 0, resource: sampler },
          { binding: 1, resource: { buffer: blurParamsBuffer } },
        ],
      });
      const createComputeBindGroup = (
        input: GPUTexture,
        output: GPUTexture,
        flip: GPUBuffer,
      ) =>
        device.createBindGroup({
          layout: blurPipeline.getBindGroupLayout(1),
          entries: [
            { binding: 1, resource: input.createView() },
            { binding: 2, resource: output.createView() },
            { binding: 3, resource: { buffer: flip } },
          ],
        });
      const computeBindGroups = [
        createComputeBindGroup(
          sourceTexture,
          intermediateTextures[0],
          horizontalFlip,
        ),
        createComputeBindGroup(
          intermediateTextures[0],
          intermediateTextures[1],
          verticalFlip,
        ),
        createComputeBindGroup(
          intermediateTextures[1],
          intermediateTextures[0],
          horizontalFlip,
        ),
      ] as const;
      const showResultBindGroup = device.createBindGroup({
        layout: fullscreenPipeline.getBindGroupLayout(0),
        entries: [
          { binding: 0, resource: sampler },
          { binding: 1, resource: intermediateTextures[1].createView() },
        ],
      });

      let previousFilterSize = 0;
      render = () => {
        if (cancelled) {
          return;
        }

        const settings = settingsRef.current;
        const plan = createBlurPassPlan({
          width: image.width,
          height: image.height,
          filterSize: settings.filterSize,
          iterations: settings.iterations,
        });
        if (settings.filterSize !== previousFilterSize) {
          device.queue.writeBuffer(
            blurParamsBuffer,
            0,
            new Uint32Array([plan.filterDim, plan.blockDim]),
          );
          previousFilterSize = settings.filterSize;
        }

        const commandEncoder = device.createCommandEncoder();
        const computePass = commandEncoder.beginComputePass();
        computePass.setPipeline(blurPipeline);
        computePass.setBindGroup(0, computeConstants);
        for (const pass of plan.passes) {
          computePass.setBindGroup(1, computeBindGroups[pass.bindGroup]);
          computePass.dispatchWorkgroups(pass.x, pass.y);
        }
        computePass.end();

        const renderPass = commandEncoder.beginRenderPass({
          colorAttachments: [
            {
              view: context.getCurrentTexture().createView(),
              clearValue: [0, 0, 0, 1],
              loadOp: "clear",
              storeOp: "store",
            },
          ],
        });
        renderPass.setPipeline(fullscreenPipeline);
        renderPass.setBindGroup(0, showResultBindGroup);
        renderPass.draw(6);
        renderPass.end();

        device.queue.submit([commandEncoder.finish()]);
        context.present();
      };

      renderRef.current = render;
      render();
    };

    void start();
    return () => {
      cancelled = true;
      if (renderRef.current === render) {
        renderRef.current = null;
      }
      for (const buffer of buffers) {
        buffer.destroy();
      }
      for (const texture of textures) {
        texture.destroy();
      }
    };
  }, [device]);

  useEffect(() => {
    renderRef.current?.();
  }, [filterSize, iterations]);

  return (
    <View style={styles.container}>
      <Canvas ref={canvasRef} style={StyleSheet.absoluteFill} />
      <View style={styles.controls}>
        <Pressable
          accessibilityRole="button"
          style={styles.button}
          onPress={() =>
            setFilterSize((current) => nextOption(FILTER_SIZES, current))
          }
        >
          <Text style={styles.buttonLabel}>Kernel</Text>
          <Text style={styles.buttonValue}>{filterSize}</Text>
        </Pressable>
        <Pressable
          accessibilityRole="button"
          style={styles.button}
          onPress={() =>
            setIterations((current) => nextOption(ITERATIONS, current))
          }
        >
          <Text style={styles.buttonLabel}>Passes</Text>
          <Text style={styles.buttonValue}>{iterations * 2}</Text>
        </Pressable>
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: "black",
  },
  controls: {
    position: "absolute",
    top: 24,
    left: 16,
    flexDirection: "row",
    gap: 8,
  },
  button: {
    minWidth: 84,
    borderRadius: 12,
    paddingHorizontal: 12,
    paddingVertical: 8,
    backgroundColor: "rgba(0,0,0,0.65)",
  },
  buttonLabel: {
    color: "rgba(255,255,255,0.65)",
    fontSize: 11,
    textTransform: "uppercase",
  },
  buttonValue: {
    color: "white",
    marginTop: 2,
    fontSize: 16,
    fontWeight: "600",
  },
});
