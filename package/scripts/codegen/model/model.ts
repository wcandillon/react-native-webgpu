import _ from "lodash";
import type {
  InterfaceDeclaration,
  MethodSignature,
  PropertySignature,
} from "ts-morph";

import { dawn, resolved } from "./dawn";

const hasPropery = <O, T extends string>(
  object: unknown,
  property: T,
): object is O & Record<T, unknown> => {
  return typeof object === "object" && object !== null && property in object;
};

const aliases: Record<string, string> = {
  GPU: "instance",
  GPUCanvasContext: "surface",
};

const toNativeName = (name: string, dependencies?: string[]) => {
  const base = `wgpu::${_.upperFirst(_.camelCase(name))}`;
  const depName = `GPU${_.upperFirst(_.camelCase(name))}`;
  if (dependencies) {
    dependencies.push(depName);
  }
  return dependencies ? `std::shared_ptr<${depName}>` : base;
};

const resolveRequiredType = (
  name: keyof typeof dawn,
  dependencies: string[],
) => {
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
  return toNativeName(name, dependencies);
};

const resolveType = (
  name: keyof typeof dawn,
  dependencies: string[],
  optional = false,
) => {
  const type = resolveRequiredType(name, dependencies);
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
  const className = (
    methodSignature.getParent() as InterfaceDeclaration
  ).getName();
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
  const dependencies: string[] = [];
  const returns = resolveType(
    modelMethod.returns as keyof typeof dawn,
    dependencies,
  );
  const args = (hasPropery(modelMethod, "args") ? modelMethod.args : []).map(
    ({ name, type }) => ({
      name,
      type: resolveType(
        type as keyof typeof dawn,
        dependencies,
        methodSignature.getParameter(name)?.isOptional(),
      ),
    }),
  );
  return {
    dependencies,
    name: methodName,
    args,
    returns,
  };
};

export const resolveProperty = (propertySignature: PropertySignature) => {
  const className = (
    propertySignature.getParent() as InterfaceDeclaration
  ).getName();
  const propertyName = propertySignature.getName();
  // If we have it resolved already, return it
  if (resolved[className]) {
    const property = resolved[className].properties.find(
      ({ name }: { name: string }) => {
        const result = name === propertyName;
        return result;
      },
    );
    if (property) {
      return property;
    }
  }
  const propertyModelName = getModelName(propertyName);
  //const classMethodName = getModelName(className);

  //const native = dawn[classMethodName as keyof typeof dawn];
  throw new Error(
    `No native method found for ${className}: ${propertyModelName}`,
  );
};

export const resolveExtra = (className: string) => {
  return resolved[className]?.extra ?? "";
};

export const resolveCtor = (className: string): string | undefined => {
  return resolved[className]?.ctor;
};
