import type * as webidl2 from "webidl2";

import { parseIDL } from "./parse";

export type Dictionary = webidl2.DictionaryType;
export type Interface = webidl2.InterfaceType;
export type Mixin = webidl2.InterfaceMixinType;
export type Enum = webidl2.EnumType;
export type Namespace = webidl2.NamespaceType;
export type Typedef = webidl2.TypedefType;

export interface IdlModel {
  enums: Map<string, Enum>;
  dictionaries: Map<string, Dictionary>;
  interfaces: Map<string, Interface>;
  namespaces: Map<string, Namespace>;
  typedefs: Map<string, Typedef>;
  // Resolved typedef name -> the idlType it aliases (for numeric typedefs such
  // as GPUSize64, and for the GPUColor/GPUExtent3D union typedefs).
  typedefMap: Map<string, webidl2.IDLTypeDescription>;
}

// Merge a duplicate/partial definition into an existing one by concatenating
// members. This mirrors how Dawn's idlgen treats repeated `enum`, `dictionary`
// and `interface` blocks (used heavily by DawnExtensions.idl) as additive.
const mergeMembers = <T extends { members: unknown[] }>(into: T, from: T) => {
  into.members.push(...from.members);
};

export const buildModel = (): IdlModel => {
  const roots = parseIDL();

  const enums = new Map<string, Enum>();
  const dictionaries = new Map<string, Dictionary>();
  const interfaces = new Map<string, Interface>();
  const mixins = new Map<string, Mixin>();
  const namespaces = new Map<string, Namespace>();
  const typedefs = new Map<string, Typedef>();
  const includes: webidl2.IncludesType[] = [];

  const upsert = <T extends { name: string; members: unknown[] }>(
    map: Map<string, T>,
    node: T,
  ) => {
    const existing = map.get(node.name);
    if (existing) {
      mergeMembers(existing, node);
    } else {
      map.set(node.name, node);
    }
  };

  for (const root of roots) {
    switch (root.type) {
      case "enum": {
        const existing = enums.get(root.name);
        if (existing) {
          existing.values.push(...root.values);
        } else {
          enums.set(root.name, root);
        }
        break;
      }
      case "dictionary":
        upsert(dictionaries, root);
        break;
      case "interface":
        upsert(interfaces, root);
        break;
      case "interface mixin":
        upsert(mixins, root);
        break;
      case "namespace":
        upsert(namespaces, root);
        break;
      case "typedef":
        typedefs.set(root.name, root);
        break;
      case "includes":
        includes.push(root);
        break;
      default:
        break;
    }
  }

  // Apply `X includes Mixin;` by folding the mixin members into the interface.
  for (const inc of includes) {
    const target = interfaces.get(inc.target);
    const mixin = mixins.get(inc.includes);
    if (target && mixin) {
      target.members.push(...(mixin.members as typeof target.members));
    }
  }

  // Flatten dictionary inheritance (`dictionary A : B`) so each descriptor
  // carries the full set of members, matching the previous codegen's
  // mergeParentInterfaces behaviour.
  const flattenDict = (dict: Dictionary, seen = new Set<string>()) => {
    if (seen.has(dict.name) || !dict.inheritance) {
      return;
    }
    seen.add(dict.name);
    const parent = dictionaries.get(dict.inheritance);
    if (parent) {
      flattenDict(parent, seen);
      // Prepend parent members that are not already present.
      const own = new Set(dict.members.map((m) => m.name));
      const inherited = parent.members.filter((m) => !own.has(m.name));
      dict.members.unshift(...inherited);
    }
  };
  for (const dict of dictionaries.values()) {
    flattenDict(dict);
  }

  const typedefMap = new Map<string, webidl2.IDLTypeDescription>();
  for (const [name, td] of typedefs) {
    typedefMap.set(name, td.idlType);
  }

  return {
    enums,
    dictionaries,
    interfaces,
    namespaces,
    typedefs,
    typedefMap,
  };
};
