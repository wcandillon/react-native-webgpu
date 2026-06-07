import { HomeLayout } from "fumadocs-ui/layouts/home";
import type { ReactNode } from "react";

import { homeOptions } from "@/lib/layout.shared";

export default function Layout({ children }: { children: ReactNode }) {
  return (
    <HomeLayout
      {...homeOptions()}
      className="rnwgpu-home-shell flex h-dvh flex-col overflow-hidden"
    >
      <div className="flex min-h-0 flex-1 flex-col">{children}</div>
    </HomeLayout>
  );
}
