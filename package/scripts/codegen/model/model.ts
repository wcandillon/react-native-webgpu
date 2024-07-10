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

const extensions: Record<string, { methods: NativeMethod[] }> = {
  GPU: {
    methods: [
      {
        name: "get preferred format",
        returns: "texture format",
        args: [{ name: "adapter", type: "adapter" }],
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
  const methodModelName = getModelName(methodName);
  const classMethodName = getModelName(className);
  const native = dawn[classMethodName as keyof typeof dawn];
  if (typeof native !== "object" || native === null || !("methods" in native)) {
    throw new Error(
      `No native method found for ${className}: ${methodModelName}`,
    );
  }

  let modelMethod = native.methods.find(({ name }: { name: string }) => {
    const result = name === methodModelName;
    return result;
  });
  if (!modelMethod) {
    modelMethod = extensions[classMethodName].methods.find(
      ({ name }: { name: string }) => {
        const result = name === methodModelName;
        return result;
      },
    );
  }
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
