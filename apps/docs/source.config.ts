import { createGenerator, remarkAutoTypeTable } from "fumadocs-typescript";
import {
  rehypeCodeDefaultOptions,
  remarkMdxMermaid,
} from "fumadocs-core/mdx-plugins";
import { defineConfig, defineDocs } from "fumadocs-mdx/config";
import { transformerTwoslash } from "fumadocs-twoslash";

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
    rehypeCodeOptions: {
      ...rehypeCodeDefaultOptions,
      transformers: [
        ...(rehypeCodeDefaultOptions.transformers ?? []),
        transformerTwoslash({
          twoslashOptions: {
            compilerOptions: {
              target: 99 /* ESNext */,
              jsx: 4 /* react-jsx */,
              esModuleInterop: true,
              // Passed programmatically, `lib` needs full file names, not the
              // tsconfig.json shorthand ("dom" → "lib.dom.d.ts").
              lib: ["lib.dom.d.ts", "lib.dom.iterable.d.ts", "lib.esnext.d.ts"],
              types: ["@webgpu/types"],
            },
          },
        }),
      ],
    },
  },
});
