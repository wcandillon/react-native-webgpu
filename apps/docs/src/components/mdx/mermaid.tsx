"use client";

import { useEffect, useId, useRef, useState } from "react";
import { useTheme } from "next-themes";

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
        theme: resolvedTheme === "dark" ? "dark" : "default",
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
    <div className="not-prose my-6 overflow-x-auto rounded-xl border border-fd-border bg-fd-card/50 p-4 shadow-sm">
      <div
        ref={containerRef}
        className="flex min-w-fit justify-center [&_svg]:max-w-none"
        dangerouslySetInnerHTML={{ __html: svg }}
      />
    </div>
  );
}
