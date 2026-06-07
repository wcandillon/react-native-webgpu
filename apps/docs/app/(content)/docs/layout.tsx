import { DocsLayout } from "fumadocs-ui/layouts/docs";
import type { ReactNode } from "react";

import {
  docsLayoutContainerClassName,
  docsLayoutSidebarOptions,
  sideBarOptions,
} from "@/lib/layout.shared";
import { source } from "@/lib/source";

export default function Layout({ children }: { children: ReactNode }) {
  return (
    <DocsLayout
      tree={source.getPageTree()}
      {...sideBarOptions()}
      sidebar={docsLayoutSidebarOptions()}
      containerProps={{ className: docsLayoutContainerClassName }}
    >
      {children}
    </DocsLayout>
  );
}
