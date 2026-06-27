"use client";

import dynamic from "next/dynamic";
import Link from "next/link";
import { useTheme } from "next-themes";

import { homeLinks } from "@/lib/layout.shared";

const HeroShader = dynamic(
  () => import("@/components/HeroShader").then((m) => m.HeroShader),
  { ssr: false },
);

const links = homeLinks();

export default function HomePage() {
  const { resolvedTheme } = useTheme();
  const isLight = resolvedTheme === "light";

  return (
    <main className="relative min-h-0 flex-1 w-full overflow-hidden">
      <HeroShader className="absolute inset-0 h-full w-full" />

      <div className="pointer-events-none absolute inset-0 z-10 flex flex-col items-center justify-center px-6 text-center">
        <h1
          className={`max-w-3xl text-4xl font-semibold tracking-tight sm:text-5xl md:text-6xl ${
            isLight ? "text-neutral-900" : "text-white drop-shadow-[0_1px_12px_rgba(0,0,0,0.55)]"
          }`}
        >
          React Native WebGPU
        </h1>
        <p
          className={`mt-5 max-w-xl text-base sm:text-lg ${
            isLight ? "text-neutral-600" : "text-white/70 drop-shadow-[0_1px_8px_rgba(0,0,0,0.45)]"
          }`}
        >
          The most powerful GPU library for React Native. Dawn-backed WebGPU on
          iOS, Android, macOS, and visionOS.
        </p>
        <div className="pointer-events-auto mt-10 flex flex-wrap items-center justify-center gap-3">
          <Link
            href={links.gettingStarted}
            className={
              isLight
                ? "rounded-lg bg-neutral-900 px-5 py-2.5 text-sm font-medium text-white transition hover:bg-neutral-800"
                : "rounded-lg bg-white px-5 py-2.5 text-sm font-medium text-neutral-900 transition hover:bg-white/90"
            }
          >
            Get Started
          </Link>
          <Link
            href={links.api}
            className={
              isLight
                ? "rounded-lg border border-neutral-300 bg-white/70 px-5 py-2.5 text-sm font-medium text-neutral-900 backdrop-blur transition hover:bg-white"
                : "rounded-lg border border-white/25 bg-white/5 px-5 py-2.5 text-sm font-medium text-white backdrop-blur transition hover:bg-white/10"
            }
          >
            API Reference
          </Link>
        </div>
      </div>
    </main>
  );
}
