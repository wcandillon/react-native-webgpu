import { ComputeToy, useComputeToy } from "./ComputeToy";

export const ComputeToys = () => {
  //1806
  const toy = useComputeToy(2349);
  if (!toy) {
    return null;
  }
  console.log({ toy });
  return <ComputeToy toy={toy} />;
};
