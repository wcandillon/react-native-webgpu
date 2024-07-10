import _ from "lodash";

import dawn from "../../../libs/dawn.json";
import { MethodSignature } from "ts-morph";

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
    methods: [{
      name: "get preferred format",
      returns: "texture format",
      args: [{ name: "adapter", type: "adapter" }],
    }],
  },
};

const aliases: Record<string, string> = {
  GPU: "instance",
  GPUCanvasContext: "surface",
};

const getModelName = (name: string) => {
  if (aliases[name]) {
    return aliases[name];
  }
  return _.lowerCase(
    _.startCase(_.camelCase(name.startsWith("GPU") ? name.substring(3) : name)),
  );
};

export const getModelMethod = (className: string, method: MethodSignature) => {
  const methodModelName = getModelName(method.getName());
  const classMethodName = getModelName(className);
  const native = dawn[classMethodName as keyof typeof dawn];
  if (typeof native !== 'object' || native === null || !('methods' in native)) {
    throw new Error(
      `No native method found for ${className}: ${methodModelName}`,
    );
  }

  let modelMethod = native.methods.find(
    ({ name }: { name: string }) => {
      const result = name === methodModelName;
      return result;
    },
  );
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
  return {
    returns: modelMethod.returns,
  }
};
