"use client";

import dynamic from "next/dynamic";
import Link from "next/link";
import { useEffect, useState } from "react";

import { SHADERS } from "@/components/hero/registry";
import { homeLinks } from "@/lib/layout.shared";

const HeroShader = dynamic(
  () => import("@/components/HeroShader").then((m) => m.HeroShader),
  { ssr: false },
);

const links = homeLinks();

export default function HomePage() {
  // Pick a shader at random per page load (client-only to avoid hydration
  // mismatch). No auto-rotation: reload for a different one.
  const [entry, setEntry] = useState(SHADERS[0]!);
  useEffect(() => {
    setEntry(SHADERS[Math.floor(Math.random() * SHADERS.length)]!);
  }, []);

  // Overlay contrast follows the shader's appearance, not the site theme.
  const dark = entry.appearance !== "light";

  return (
    <main className="relative min-h-0 flex-1 w-full overflow-hidden">
      <HeroShader entry={entry} className="absolute inset-0 h-full w-full" />

      <div className="pointer-events-none absolute inset-0 z-10 flex flex-col items-center justify-center px-6 text-center">
        <h1
          className={`max-w-3xl text-4xl font-semibold tracking-tight sm:text-5xl md:text-6xl ${
            dark
              ? "text-white drop-shadow-[0_1px_12px_rgba(0,0,0,0.55)]"
              : "text-neutral-900"
          }`}
        >
          React Native WebGPU
        </h1>
        <p
          className={`mt-5 max-w-xl text-base sm:text-lg ${
            dark
              ? "text-white/90 drop-shadow-[0_1px_6px_rgba(0,0,0,1)]"
              : "text-neutral-600"
          }`}
        >
          Powered by Dawn.<br />
          Runs on iOS, Android, macOS, visionOS, and Web.
        </p>
        <div className="pointer-events-auto mt-10 flex flex-wrap items-center justify-center gap-3">
          <Link
            href={links.gettingStarted}
            className={
              dark
                ? "rounded-lg bg-white px-5 py-2.5 text-sm font-medium text-neutral-900 transition hover:bg-white/90"
                : "rounded-lg bg-neutral-900 px-5 py-2.5 text-sm font-medium text-white transition hover:bg-neutral-800"
            }
          >
            Get Started
          </Link>
          <Link
            href={links.api}
            className={
              dark
                ? "rounded-lg border border-white/25 bg-white/5 px-5 py-2.5 text-sm font-medium text-white backdrop-blur transition hover:bg-white/10"
                : "rounded-lg border border-neutral-300 bg-white/70 px-5 py-2.5 text-sm font-medium text-neutral-900 backdrop-blur transition hover:bg-white"
            }
          >
            API Reference
          </Link>
        </div>
      </div>
    </main>
  );
}
