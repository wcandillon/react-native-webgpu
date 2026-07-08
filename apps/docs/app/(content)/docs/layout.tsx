import { DocsLayout } from "fumadocs-ui/layouts/docs";
import { SearchTrigger } from "fumadocs-ui/layouts/shared/slots/search-trigger";
import type { ReactNode } from "react";

import { HiddenSidebarSearch } from "@/components/sidebar-search-slot";
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
      // Keep search reachable (mobile header / Cmd+K) but hide it from the
      // sidebar by rendering nothing for the sidebar's `full` trigger.
      slots={{ searchTrigger: { sm: SearchTrigger, full: HiddenSidebarSearch } }}
      containerProps={{ className: docsLayoutContainerClassName }}
    >
      {children}
    </DocsLayout>
  );
}
