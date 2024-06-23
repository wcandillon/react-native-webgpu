import { $, checkBuildArtifacts } from "./util";

$("cp -R ../artifacts/libs .");
$("cp -R ../artifacts/cpp/dawn cpp");
$("cp -R ../artifacts/cpp/webgpu cpp");
checkBuildArtifacts();
