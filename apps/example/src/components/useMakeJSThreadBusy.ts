import { useEffect } from "react";

const fibonacci = (num: number) => {
  let a = 1,
    b = 0,
    temp;

  while (num >= 0) {
    temp = a;
    a = a + b;
    b = temp;
    num--;
  }

  return b;
};

export const useMakeJsThreadBusy = () =>
  useEffect(() => {
    setInterval(() => {
      console.log("JS thread is busy now");
      while (true) {
        fibonacci(10000);
      }
    }, 2000);
  }, []);
