"use client";

import { useEffect, useId, useRef, useState } from "react";
import { useTheme } from "next-themes";

const MONOCHROME_LIGHT = {
  primaryColor: "#f2f2f2",
  secondaryColor: "#f2f2f2",
  tertiaryColor: "#f2f2f2",
  primaryBorderColor: "#d4d4d4",
  secondaryBorderColor: "#d4d4d4",
  tertiaryBorderColor: "#d4d4d4",
  primaryTextColor: "#171717",
  secondaryTextColor: "#171717",
  tertiaryTextColor: "#171717",
  textColor: "#404040",
  lineColor: "#a3a3a3",
  // The figure's background, so the labels read as text on it.
  edgeLabelBackground: "#ffffff",
  clusterBkg: "#fafafa",
  clusterBorder: "#d4d4d4",
};

const MONOCHROME_DARK = {
  primaryColor: "#262626",
  secondaryColor: "#262626",
  tertiaryColor: "#262626",
  primaryBorderColor: "#404040",
  secondaryBorderColor: "#404040",
  tertiaryBorderColor: "#404040",
  primaryTextColor: "#f5f5f5",
  secondaryTextColor: "#f5f5f5",
  tertiaryTextColor: "#f5f5f5",
  textColor: "#d4d4d4",
  lineColor: "#737373",
  edgeLabelBackground: "#191919",
  clusterBkg: "#1a1a1a",
  clusterBorder: "#404040",
};

export function Mermaid({ chart }: { chart: string }) {
  const [mounted, setMounted] = useState(false);

  useEffect(() => {
    setMounted(true);
  }, []);

  if (!mounted) {
    return (
      <div
        aria-hidden
        className="my-6 h-48 animate-pulse rounded-xl border border-fd-border bg-fd-muted/30"
      />
    );
  }

  return <MermaidContent chart={chart} />;
}

function MermaidContent({ chart }: { chart: string }) {
  const id = useId();
  const containerRef = useRef<HTMLDivElement>(null);
  const { resolvedTheme } = useTheme();
  const [svg, setSvg] = useState("");

  useEffect(() => {
    let cancelled = false;

    async function renderChart() {
      const { default: mermaid } = await import("mermaid");

      mermaid.initialize({
        startOnLoad: false,
        securityLevel: "loose",
        fontFamily: "inherit",
        themeCSS: "margin: 0 auto;",
        // Monochrome, to match the neutral palette of the docs: Mermaid's
        // own themes are colored, and it cannot parse the theme's CSS colors.
        theme: "base",
        themeVariables:
          resolvedTheme === "dark" ? MONOCHROME_DARK : MONOCHROME_LIGHT,
        flowchart: {
          htmlLabels: true,
          curve: "basis",
          padding: 16,
          nodeSpacing: 40,
          rankSpacing: 56,
        },
      });

      try {
        const { svg, bindFunctions } = await mermaid.render(
          id.replaceAll(":", ""),
          chart.replaceAll("\\n", "\n"),
          containerRef.current ?? undefined,
        );

        if (cancelled) return;

        setSvg(svg);

        if (containerRef.current) {
          bindFunctions?.(containerRef.current);
        }
      } catch (error) {
        console.error("Mermaid render error:", error);
      }
    }

    void renderChart();

    return () => {
      cancelled = true;
    };
  }, [chart, id, resolvedTheme]);

  return (
    <div className="not-prose my-6 overflow-x-auto rounded-xl border border-fd-border bg-white p-4 dark:bg-fd-card">
      <div
        ref={containerRef}
        className="flex min-w-fit justify-center [&_svg]:max-w-none"
        dangerouslySetInnerHTML={{ __html: svg }}
      />
    </div>
  );
}
