import { api, docs } from "collections/server";
import { loader } from "fumadocs-core/source";

export const searchSource = loader(
  {
    docs: docs.toFumadocsSource(),
    api: api.toFumadocsSource(),
  },
  { baseUrl: "/" },
);
