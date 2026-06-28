"use client";

import { useEffect, useRef } from "react";
import { useTheme } from "next-themes";

import { HeroCredit } from "./hero/HeroCredit";
import { HERO_UNIFORM_SIZE } from "./hero/prelude";
import { createPass, type Pass } from "./hero/runner";
import type { HeroContext, ShaderEntry } from "./hero/types";

interface HeroShaderProps {
  entry: ShaderEntry;
  className?: string;
}

export function HeroShader({ entry, className }: HeroShaderProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const { resolvedTheme } = useTheme();
  const themeTargetRef = useRef(0);
  const themeRef = useRef(0);

  // The `theme` uniform tracks the site light/dark mode for shaders that adapt
  // to it (the compute.toys shaders ignore it).
  useEffect(() => {
    themeTargetRef.current = resolvedTheme === "light" ? 1 : 0;
  }, [resolvedTheme]);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas || !navigator.gpu) {
      return;
    }

    let animationId = 0;
    let disposed = false;
    let device: GPUDevice | null = null;
    let uniformBuffer: GPUBuffer | null = null;
    let pass: Pass | null = null;

    (async () => {
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter || disposed) {
        return;
      }
      device = await adapter.requestDevice();
      if (disposed) {
        device.destroy();
        return;
      }

      const context = canvas.getContext("webgpu");
      if (!context) {
        return;
      }

      const format = navigator.gpu.getPreferredCanvasFormat();
      context.configure({ device, format, alphaMode: "opaque" });

      const ctx: HeroContext = { device, format, canvas };
      uniformBuffer = device.createBuffer({
        size: HERO_UNIFORM_SIZE,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
      });

      const resize = () => {
        if (disposed || !device) {
          return;
        }
        const dpr = window.devicePixelRatio || 1;
        const { width, height } = canvas.getBoundingClientRect();
        const w = Math.max(1, Math.floor(width * dpr));
        const h = Math.max(1, Math.floor(height * dpr));
        if (canvas.width !== w || canvas.height !== h) {
          canvas.width = w;
          canvas.height = h;
          context.configure({ device, format, alphaMode: "opaque" });
        }
        pass?.resize(canvas.width, canvas.height);
      };

      window.addEventListener("resize", resize);
      resize();

      pass = createPass(ctx, entry.shader, uniformBuffer);
      pass.resize(canvas.width, canvas.height);

      const shaderStart = performance.now();
      let frame = 0;
      const uniforms = new Float32Array(HERO_UNIFORM_SIZE / 4);

      const render = (now: number) => {
        if (disposed || !device || !uniformBuffer) {
          return;
        }

        resize();

        const dt = 1 / 60;
        themeRef.current +=
          (themeTargetRef.current - themeRef.current) * (1 - Math.exp(-dt * 10));
        const theme = themeRef.current;
        const time = (now - shaderStart) / 1000;
        const dpr = window.devicePixelRatio || 1;

        uniforms[0] = canvas.width;
        uniforms[1] = canvas.height;
        uniforms[2] = time;
        uniforms[3] = theme;
        uniforms[4] = frame;
        uniforms[5] = dpr;
        device.queue.writeBuffer(uniformBuffer, 0, uniforms);

        pass?.update(frame, time);

        const encoder = device.createCommandEncoder();
        pass?.encode(encoder, context.getCurrentTexture().createView());
        device.queue.submit([encoder.finish()]);

        frame += 1;
        animationId = requestAnimationFrame(render);
      };

      animationId = requestAnimationFrame(render);

      return () => {
        window.removeEventListener("resize", resize);
      };
    })();

    return () => {
      disposed = true;
      cancelAnimationFrame(animationId);
      pass?.destroy();
      uniformBuffer?.destroy();
      device?.destroy();
    };
  }, [entry]);

  const dark = entry.appearance !== "light";

  return (
    <div className={className}>
      <canvas
        ref={canvasRef}
        className="block h-full w-full"
        style={{ background: dark ? "#050508" : "#f6f6fa" }}
      />
      <HeroCredit entry={entry} dark={dark} />
    </div>
  );
}
