/* eslint-disable @typescript-eslint/no-explicit-any */

import _ from "lodash";
import type {
  MethodSignature,
  PropertySignature,
  Type,
  InterfaceDeclaration,
} from "ts-morph";
import { SyntaxKind } from "ts-morph";

import dawn from "../../../libs/dawn.json";

export const mergeParentInterfaces = (interfaceDecl: InterfaceDeclaration) => {
  if (interfaceDecl.getKind() !== SyntaxKind.InterfaceDeclaration) {
    return interfaceDecl;
  }
  const parentInterfaces = interfaceDecl.getBaseDeclarations();
  for (const parentInterface of parentInterfaces) {
    const parentDeclaration = parentInterface
      .getType()
      .getSymbol()
      ?.getDeclarations()[0];

    if (parentDeclaration) {
      const parentInterfaceDecl = parentDeclaration as InterfaceDeclaration;

      // Recursively merge parent interfaces
      const mergedParentInterface = mergeParentInterfaces(parentInterfaceDecl);

      // Merge properties from parent to child
      for (const prop of mergedParentInterface.getProperties()) {
        if (!interfaceDecl.getProperty(prop.getName())) {
          interfaceDecl.addProperty(prop.getStructure());
        }
      }

      // Merge methods from parent to child
      for (const method of mergedParentInterface.getMethods()) {
        if (!interfaceDecl.getMethod(method.getName())) {
          interfaceDecl.addMethod(method.getStructure());
        }
      }
    }
  }

  return interfaceDecl;
};

export const getJSIProp = (method: PropertySignature) => {
  const name = method.getName();
  const { type, dependencies } = getType(method.getType());
  return { name, type, dependencies };
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

interface JsiMethod {
  async: boolean;
  name: string;
  apiName: string;
  dependencies: string[];
  args: {
    name: string;
    type: string;
    optional: boolean;
    defaultValue: string | undefined;
  }[];
  argNames: string[];
  returns: string;
  wgpuReturns: string;
}

export const getJSIMethod = (
  className: string,
  method: MethodSignature,
): JsiMethod => {
  const methodModelName = getModelName(method.getName());
  const classMethodName = getModelName(className);
  const native = dawn[classMethodName as keyof typeof dawn];
  if (!native) {
    throw new Error(
      `No native method found for ${className}: ${methodModelName}`,
    );
  }

  const modelMethod = (native as any).methods.find(
    ({ name }: { name: string }) => {
      const result = name === methodModelName;
      return result;
    },
  );
  const async = method.getReturnType().getSymbol()?.getName() === "Promise";
  const name = method.getName();
  const apiName = _.upperFirst(name);
  const { type: returns, dependencies } = getType(method.getReturnType()!);
  const args: {
    name: string;
    type: string;
    optional: boolean;
    defaultValue: undefined | string;
  }[] = method.getParameters().map((p) => {
    const { type, dependencies: deps } = getType(p.getType());
    dependencies.push(...deps);
    const modelArg = modelMethod?.args.find(
      ({ name: argName }: { name: string }) =>
        argName === getModelName(p.getName()),
    );
    const defaultValue = modelArg?.default;
    return {
      type,
      name: p.getName(),
      optional: p.isOptional(),
      defaultValue,
    };
  });
  const argNames: string[] = method
    .getParameters()
    .map((p) => `${p.getName()}`);
  return {
    async,
    apiName,
    name,
    dependencies,
    args,
    argNames,
    returns,
    wgpuReturns: returns.startsWith("GPU")
      ? `wgpu::${returns.substring(3)}`
      : returns,
  };
};

const getType = (
  type: Type,
  dependencies: string[] = [],
): { type: string; dependencies: string[] } => {
  if (type.isNumber()) {
    return { type: "double", dependencies };
  } else if (type.isString()) {
    return { type: "std::string", dependencies };
  } else if (type.isBoolean()) {
    return { type: "bool", dependencies };
  } else if (type.isUnion()) {
    return getType(
      type.getUnionTypes().filter((t) => !t.isNull() && !t.isUndefined())[0],
      dependencies,
    );
  } else if (type.getTypeArguments()[0]) {
    return getType(type.getTypeArguments()[0], dependencies);
  } else {
    let textType = type.getText();
    if (textType.startsWith("GPU")) {
      dependencies.push(textType);
    } else if (textType === "ArrayBuffer") {
      textType = "MutableJSIBuffer";
    }
    return { type: textType, dependencies };
  }
};

export const wrapType = (type: string, optional = false) => {
  const result =
    type.startsWith("GPU") || type === "MutableJSIBuffer"
      ? `std::shared_ptr<${type.startsWith("GPU") ? `wgpu::${type.substring(3)}` : type}>`
      : type;
  return optional ? `std::optional<${result}>` : result;
};
