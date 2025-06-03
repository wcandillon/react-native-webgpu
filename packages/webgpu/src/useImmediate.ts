import { useReducer, useState } from "react";

export interface Immediate<T> {
  /** Get latest value */
  get(): T;
  set(value: T): void;
}

function emptyReducer<T>(value: T): T {
  return value;
}

interface CreateImmediateOptions<T> {
  readonly initialValue: T;
  setValue(v: T): void;
}

function createImmediate<T>(options: CreateImmediateOptions<T>): Immediate<T> {
  let value = options.initialValue;

  return {
    get(): T {
      return value;
    },
    set(newValue: T): void {
      value = newValue;
      options.setValue(newValue);
    },
  };
}

/**
 * A replacement for `useState` that returns an "Immediate", which in this case is an object
 * that allows to read the latest value, and to write a new value. Its object identity
 * is stable, and does not change when writing a new value. That also means that it's
 * not reactive (you can't depend on it in useMemo or useEffect). For that, use the
 * second value from the returned tuple.
 * @param initialValue 
 * @returns 
 */
export function useImmediate<T>(initialValue: T): [Immediate<T>, T] {
  const [value, setValue] = useState(initialValue);
  const [immediate] = useReducer(emptyReducer<Immediate<T>>, { initialValue, setValue }, createImmediate<T>);

  return [immediate, value] as const;
}
