import { api, docs } from "collections/server";
import { loader, type Source } from "fumadocs-core/source";

// Mount each collection under the same prefix as its page loader ("/docs" in
// source.ts, "/api" in api-source.ts) so search result URLs match the routes.
// Without the prefix both collections share the root and their slugs collide.
function prefixed<S extends Source>(prefix: string, source: S): S {
  return {
    ...source,
    files: source.files.map((file) => ({
      ...file,
      path: `${prefix}/${file.path}`,
    })),
  };
}

export const searchSource = loader({
  baseUrl: "/",
  source: {
    files: [
      ...prefixed("docs", docs.toFumadocsSource()).files,
      ...prefixed("api", api.toFumadocsSource()).files,
    ],
  },
});
