import { HomeLayout } from "fumadocs-ui/layouts/home";
import type { ReactNode } from "react";

import { docsShellClassName, homeOptions } from "@/lib/layout.shared";

export default function ContentLayout({ children }: { children: ReactNode }) {
  return (
    <HomeLayout {...homeOptions()} className={docsShellClassName}>
      {children}
    </HomeLayout>
  );
}
