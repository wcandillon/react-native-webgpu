import _ from "lodash";

import { dawn } from "./dawn";
import { InterfaceDeclaration, MethodSignature } from "ts-morph";

const hasPropery = <O, T extends string>(
  object: unknown,
  property: T,
): object is O & Record<T, unknown> => {
  return typeof object === "object" && object !== null && property in object;
};

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
        name: "getPreferredCanvasFormat",
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

const toNativeName = (name: string, sharedPtr = false) => {
  const base = `wgpu::${_.upperFirst(_.camelCase(name))}`;
  return sharedPtr
    ? `std::shared_ptr<GPU${_.upperFirst(_.camelCase(name))}>`
    : base;
};

const resolveRequiredType = (name: keyof typeof dawn) => {
  const type = dawn[name];
  if (!hasPropery(type, "category")) {
    return "void";
  }
  if (type.category === "native") {
    if (name === "void *") {
      return "std::shared_ptr<MutableJSIBuffer>";
    }
    return name;
  }
  if (type.category === "enum") {
    return toNativeName(name);
  }
  return toNativeName(name, true);
};

const resolveType = (name: keyof typeof dawn, optional = false) => {
  const type = resolveRequiredType(name);
  if (optional) {
    return `std::optional<${type}>`;
  }
  return type;
};

const getModelName = (name: string) => {
  if (aliases[name]) {
    return aliases[name];
  }
  return _.lowerCase(
    _.startCase(_.camelCase(name.startsWith("GPU") ? name.substring(3) : name)),
  );
};

export const resolveMethod = (methodSignature: MethodSignature) => {
  const className = (methodSignature.getParent() as InterfaceDeclaration).getName();
  const methodName = methodSignature.getName();
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
  if (!hasPropery(native, "methods")) {
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
  return {
    args: (hasPropery(modelMethod, "args") ? modelMethod.args : []).map(
      ({ name, type }) => ({
        name,
        type: resolveType(type as keyof typeof dawn, methodSignature.getParameter(name)?.isOptional()),
      }),
    ),
    returns: resolveType(modelMethod.returns as keyof typeof dawn),
  };
};
