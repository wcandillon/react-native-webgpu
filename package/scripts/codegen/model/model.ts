import _ from "lodash";

import { dawn } from "./dawn";

interface NativeMethod {
  name: string;
  args: {
    name: string;
    type: string;
  }[];
  returns: string;
}

const resolved: Record<string, { methods: NativeMethod[] }> = {
  GPU: {
    methods: [
      {
        name: "getPreferredFormat",
        returns: "wgpu::TextureFormat",
        args: [],
      },
      {
        name: "requestAdapter",
        args: [
          {
            name: "options",
            type: "std::shared_ptr<GPURequestAdapterOptions> ",
          },
        ],
        returns: "std::future<std::shared_ptr<GPUAdapter>>",
      },
    ],
  },
};

const aliases: Record<string, string> = {
  GPU: "instance",
  GPUCanvasContext: "surface",
};

const toNativeName = (name: string) =>
  `wgpu::${_.upperFirst(_.camelCase(name))}`;

const resolveType = (name: keyof typeof dawn) => {
  const type = dawn[name];
  if (
    !type ||
    typeof type !== "object" ||
    type === null ||
    !("category" in type) ||
    type.category !== "enum"
  ) {
    throw new Error(`No type found for ${name}`);
  }
  return toNativeName(name);
};

const getModelName = (name: string) => {
  if (aliases[name]) {
    return aliases[name];
  }
  return _.lowerCase(
    _.startCase(_.camelCase(name.startsWith("GPU") ? name.substring(3) : name)),
  );
};

export const resolveMethod = (className: string, methodName: string) => {
  // If we have it resolved already, return it
  if (resolved[className]) {
    const method = resolved[className].methods.find(
      ({ name }: { name: string }) => {
        const result = name === methodName;
        return result;
      },
    );
    if (method) {
      return method;
    }
  }
  const methodModelName = getModelName(methodName);
  const classMethodName = getModelName(className);
  const native = dawn[classMethodName as keyof typeof dawn];
  if (typeof native !== "object" || native === null || !("methods" in native)) {
    throw new Error(
      `No native method found for ${className}: ${methodModelName}`,
    );
  }

  const modelMethod = native.methods.find(({ name }: { name: string }) => {
    const result = name === methodModelName;
    return result;
  });

  if (!modelMethod) {
    throw new Error(
      `No model method found for ${className}: ${methodModelName}`,
    );
  }
  if (!modelMethod.returns) {
    throw new Error(
      `No return type found for ${className}: ${methodModelName}: JSON: ${JSON.stringify(modelMethod)}`,
    );
  }
  return {
    returns: resolveType(modelMethod.returns as keyof typeof dawn),
  };
};
