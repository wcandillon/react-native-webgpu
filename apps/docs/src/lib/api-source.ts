import { api } from "collections/server";
import { loader } from "fumadocs-core/source";

export const apiSource = loader({
  baseUrl: "/api",
  source: api.toFumadocsSource(),
});
