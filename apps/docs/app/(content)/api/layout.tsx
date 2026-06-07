import { DocsLayout } from "fumadocs-ui/layouts/docs";
import type { ReactNode } from "react";

import {
  apiReferenceSidebarWidthClassName,
  docsLayoutContainerClassName,
  docsLayoutSidebarOptions,
  sideBarOptions,
} from "@/lib/layout.shared";
import { apiSource } from "@/lib/api-source";

export default function Layout({ children }: { children: ReactNode }) {
  return (
    <DocsLayout
      tree={apiSource.getPageTree()}
      {...sideBarOptions()}
      sidebar={docsLayoutSidebarOptions()}
      containerProps={{
        className: `${docsLayoutContainerClassName} ${apiReferenceSidebarWidthClassName}`,
      }}
    >
      {children}
    </DocsLayout>
  );
}
