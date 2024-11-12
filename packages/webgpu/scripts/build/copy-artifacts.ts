import { checkBuildArtifacts } from "./dawn-configuration";
import { $ } from "./util";

$("cp -R ../../artifacts/libs .");
$("cp -R ../../artifacts/cpp/webgpu cpp");
$("cp -R ../../artifacts/cpp/dawn cpp");
checkBuildArtifacts();
