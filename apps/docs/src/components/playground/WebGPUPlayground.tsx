"use client";

import dynamic from "next/dynamic";
import type { ReactNode } from "react";

const PlaygroundInner = dynamic(() => import("./PlaygroundInner"), {
  ssr: false,
  loading: () => (
    <div className="flex h-64 items-center justify-center rounded-xl border bg-fd-muted text-sm text-fd-muted-foreground">
      Loading WebGPU playground…
    </div>
  ),
});

export type PlaygroundDemo =
  | "hello-triangle"
  | "rotating-cube"
  | "transparency"
  | "boids"
  | "particles"
  | "camera-effect"
  | "three-js"
  | "tensorflow";

interface WebGPUPlaygroundProps {
  demo: PlaygroundDemo;
  height?: number;
  transparent?: boolean;
  children?: ReactNode;
}

export function WebGPUPlayground({
  demo,
  height = 320,
  transparent,
}: WebGPUPlaygroundProps) {
  return (
    <div className="not-prose my-6 overflow-hidden rounded-xl border shadow-sm">
      <PlaygroundInner demo={demo} height={height} transparent={transparent} />
    </div>
  );
}
