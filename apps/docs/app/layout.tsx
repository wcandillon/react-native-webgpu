import "./global.css";
import { RootProvider } from "fumadocs-ui/provider/next";
import type { ReactNode } from "react";

import DocsSearchDialog from "@/components/search";

export default function Layout({ children }: { children: ReactNode }) {
  return (
    <html lang="en" suppressHydrationWarning>
      <body className="flex min-h-screen flex-col">
        <RootProvider
          search={{
            SearchDialog: DocsSearchDialog,
          }}
        >
          {children}
        </RootProvider>
      </body>
    </html>
  );
}
