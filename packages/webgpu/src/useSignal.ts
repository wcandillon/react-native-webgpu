import { useReducer, useState } from "react";

export interface Signal<T> {
  /** Get latest value */
  get(): T;
  set(value: T): void;
}

function emptyReducer<T>(value: T): T {
  return value;
}

interface CreateSignalOptions<T> {
  initialValue: T;
  setValue(v: T): void;
}

function createSignal<T>(options: CreateSignalOptions<T>): Signal<T> {
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
 * A replacement for `useState` that returns a "Signal", which in this case is an object
 * that allows to read the latest value, and to write a new value. It's object identity
 * is stable, and does not change when writing a new value. That also means that it's
 * not reactive (you can't depend on it in useMemo or useEffect). For that, use the
 * second value from the returned tuple.
 * @param initialValue 
 * @returns 
 */
export function useSignal<T>(initialValue: T): [Signal<T>, T] {
  const [value, setValue] = useState(initialValue);
  const [signal] = useReducer(emptyReducer<Signal<T>>, { initialValue, setValue }, createSignal<T>);

  return [signal, value] as const;
}
