import { ComputeToy, useComputeToy } from "./ComputeToy";

export const ComputeToys = () => {
  //1806
  const toy = useComputeToy(537);
  if (!toy) {
    return null;
  }
  return <ComputeToy toy={toy} />;
};
