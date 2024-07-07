import _ from "lodash";
import type { MethodSignature, PropertySignature, Type } from "ts-morph";

export const getJSIProp = (method: PropertySignature) => {
  const name = method.getName();
  const { type, dependencies } = getType(method.getType());
  return { name, type, dependencies };
};

interface JsiMethod {
  async: boolean;
  name: string;
  apiName: string;
  dependencies: string[];
  args: { name: string; type: string }[];
  argNames: string[];
  returns: string;
  wgpuReturns: string;
}

export const getJSIMethod = (method: MethodSignature): JsiMethod => {
  const async = method.getReturnType().getSymbol()?.getName() === "Promise";
  const name = method.getName();
  const apiName = _.upperFirst(name);
  const { type: returns, dependencies } = getType(method.getReturnType()!);
  const args: { name: string; type: string }[] = method
    .getParameters()
    .map((p) => {
      const { type, dependencies: deps } = getType(p.getType());
      dependencies.push(...deps);
      return { type, name: p.getName() };
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

export const wrapType = (type: string) => {
  return type.startsWith("GPU") || type === "MutableJSIBuffer"
    ? `std::shared_ptr<${type}>`
    : type;
};
