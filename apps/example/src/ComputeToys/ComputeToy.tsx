import { useCallback, useEffect, useRef, useState } from "react";
import type { CanvasRef } from "react-native-wgpu";
import { Canvas } from "react-native-wgpu";
import { Platform, useWindowDimensions } from "react-native";
import { useSharedValue } from "react-native-reanimated";
import { Gesture, GestureDetector } from "react-native-gesture-handler";

import { ComputeEngine } from "./engine";

export interface ComputeToy {
  shader: string;
  uniforms: Record<string, number>;
}

// Use a CORS proxy for web to bypass CORS restrictions
const getCorsProxyUrl = (url: string) => {
  return Platform.OS === "web"
    ? `https://corsproxy.io/?${encodeURIComponent(url)}`
    : url;
};

export const useComputeToy = (toyId: number) => {
  const [props, setProps] = useState<ComputeToy | null>(null);

  useEffect(() => {
    (async () => {
      const shaderURL = getCorsProxyUrl(
        `https://compute.toys/view/${toyId}/wgsl`,
      );
      const uniformsURL = getCorsProxyUrl(
        `https://compute.toys/view/${toyId}/json`,
      );

      // Execute both fetch requests in parallel
      const [shaderResponse, uniformsResponse] = await Promise.all([
        fetch(shaderURL),
        fetch(uniformsURL),
      ]);

      // Process the responses in parallel
      const [shader, uniformsJSON] = await Promise.all([
        shaderResponse.text(),
        uniformsResponse.json(),
      ]);
      const uniforms: Record<string, number> = {};
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      uniformsJSON.body.uniforms.forEach((uniform: any) => {
        uniforms[uniform.name] = uniform.value;
      });
      setProps({ shader, uniforms });
    })();
  }, [toyId]);
  return props;
};

export interface ComputeToyProps {
  toy: ComputeToy;
}

export const ComputeToy = ({ toy: { shader, uniforms } }: ComputeToyProps) => {
  const ref = useRef<CanvasRef>(null);
  const mouse = useSharedValue({ pos: { x: 0, y: 0 }, click: false });
  const { width, height } = useWindowDimensions();
  const [engine, setEngine] = useState<ComputeEngine | null>(null);
  const animationRef = useRef<number | null>(null);
  const lastTimeRef = useRef<number>(0);

  // Initialize WebGPU and the compute engine
  useEffect(() => {
    const initWebGPU = async () => {
      try {
        // Create the compute engine
        await ComputeEngine.create();
        const eng = ComputeEngine.getInstance();

        // Set the canvas surface
        if (ref.current) {
          eng.setSurface(ref.current!);

          // Set the canvas size based on your CANVAS constants
          // TODO: use PixelRatio.get()?
          eng.resize(width, height, 1);

          // Initialize render state
          eng.reset();

          // Set callbacks for shader compilation
          eng.onSuccess((entryPoints) => {
            console.log(
              "Shader compiled successfully with entry points:",
              entryPoints,
            );
          });

          eng.onError((message, row, col) => {
            console.error(`Shader error at ${row}:${col} - ${message}`);
          });
          eng.setCustomFloats(
            Object.keys(uniforms),
            new Float32Array(Object.values(uniforms)),
          );
          // Process and compile the shader
          const preprocessed = await eng.preprocess(shader);
          if (preprocessed) {
            await eng.compile(preprocessed);
          }

          setEngine(eng);
        }
      } catch (error) {
        console.error("Failed to initialize WebGPU:", error);
      }
    };

    initWebGPU();

    return () => {
      if (animationRef.current !== null) {
        cancelAnimationFrame(animationRef.current);
      }
    };
  }, [height, ref, shader, uniforms, width]);

  // Animation/render loop
  const renderLoop = useCallback(
    (timestamp: number) => {
      if (!engine) {
        return;
      }

      // Calculate time delta
      const delta = lastTimeRef.current
        ? (timestamp - lastTimeRef.current) / 1000
        : 0;
      lastTimeRef.current = timestamp;

      // Update time uniforms
      engine.setTimeElapsed(timestamp / 1000);
      engine.setTimeDelta(delta);
      if (mouse) {
        engine.setMousePos(mouse.value.pos.x, mouse.value.pos.y);
        engine.setMouseClick(mouse.value.click);
      }
      // Render frame

      engine.render();
      // Schedule next frame
      animationRef.current = requestAnimationFrame(renderLoop);
    },
    [engine, mouse],
  );

  useEffect(() => {
    renderLoop(new Date().getTime());
  }, [renderLoop]);

  const gesture = Gesture.Pan()
    .onChange((e) => {
      mouse.value = {
        pos: {
          x: e.absoluteX / width,
          y: e.absoluteY / height,
        },
        click: true,
      };
    })
    .onEnd((e) => {
      mouse.value = {
        pos: {
          x: e.absoluteX / width,
          y: e.absoluteY / height,
        },
        click: false,
      };
    });

  return (
    <GestureDetector gesture={gesture}>
      <Canvas ref={ref} style={{ width, height }} />
    </GestureDetector>
  );
};
