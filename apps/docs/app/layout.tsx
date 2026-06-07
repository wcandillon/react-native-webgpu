import "./global.css";
import { RootProvider } from "fumadocs-ui/provider/next";
import type { ReactNode } from "react";

import DocsSearchDialog from "@/components/search";

const basePath = process.env.GITHUB_PAGES === "true" ? "/react-native-webgpu" : "";

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
        {basePath ? (
          <script
            dangerouslySetInnerHTML={{
              __html: `window.__NEXT_BASE_PATH__=${JSON.stringify(basePath)};`,
            }}
          />
        ) : null}
      </body>
    </html>
  );
}
