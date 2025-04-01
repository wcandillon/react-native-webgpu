import { useCallback, useEffect, useRef, useState } from "react";
import { Canvas, useCanvasEffect } from "react-native-wgpu";
import { PixelRatio, useWindowDimensions } from "react-native";

import { ComputeEngine } from "./engine";

export interface ComputeToy {
  shader: string;
  uniforms: Record<string, number>;
}

export const useComputeToy = (toyId: number) => {
  const [props, setProps] = useState<ComputeToy | null>(null);

  useEffect(() => {
    (async () => {
      const shaderURL = `https://compute.toys/view/${toyId}/wgsl`;
      const uniformsURL = `https://compute.toys/view/${toyId}/json`;

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
  const { width, height } = useWindowDimensions();
  const [engine, setEngine] = useState<ComputeEngine | null>(null);
  const animationRef = useRef<number | null>(null);
  const lastTimeRef = useRef<number>(0);

  // Initialize WebGPU and the compute engine
  const canvasRef = useCanvasEffect(() => {
    const initWebGPU = async () => {
      try {
        // Create the compute engine
        await ComputeEngine.create();
        const eng = ComputeEngine.getInstance();

        // Set the canvas surface
        if (canvasRef.current) {
          eng.setSurface(canvasRef.current!);

          // Set the canvas size based on your CANVAS constants
          // TODO: use PixelRatio.get()
          eng.resize(width, height, PixelRatio.get());

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
  });

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
      // Render frame

      engine.render();
      // Schedule next frame
      animationRef.current = requestAnimationFrame(renderLoop);
    },
    [engine],
  );

  useEffect(() => {
    renderLoop(new Date().getTime());
  }, [renderLoop]);

  return <Canvas ref={canvasRef} style={{ width, height }} />;
};
