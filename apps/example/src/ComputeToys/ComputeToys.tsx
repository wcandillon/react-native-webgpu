import { ComputeToy, useComputeToy } from "./ComputeToy";

export const ComputeToys = () => {
  const toy = useComputeToy(202);
  if (!toy) {
    return null;
  }
  return <ComputeToy toy={toy} />;
};
