import { createGenerator, remarkAutoTypeTable } from "fumadocs-typescript";
import { remarkMdxMermaid } from "fumadocs-core/mdx-plugins";
import { defineConfig, defineDocs } from "fumadocs-mdx/config";

const generator = createGenerator({ cache: "fs" });

export const docs = defineDocs({
  dir: "content/docs",
});

export const api = defineDocs({
  dir: "content/api",
});

export default defineConfig({
  mdxOptions: {
    remarkNpmOptions: {
      persist: {
        id: "package-manager",
      },
    },
    remarkPlugins: [
      remarkMdxMermaid,
      [remarkAutoTypeTable, { generator }],
    ],
  },
});
