// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "helper.h"  // NOLINT(build/include)

#include "../sandboxed.h"  // NOLINT(build/include)
#include "sandboxed_api/sandbox2/util/fileops.h"
#include "sandboxed_api/sandbox2/util/path.h"
#include "sandboxed_api/sandbox2/util/runfiles.h"

constexpr absl::string_view kWarning =
    "Something went wrong: CWD don't contain expected dir. Please run build "
    "project in the libtiff/build dir. To run example send project dir as a "
    "parameter: ./sandboxed /absolute/path/to/project/dir .\n"
    "Falling back to using current working directory as project dir.\n";

std::string GetFilePath(absl::string_view filename) {
  std::string exec_file_path = sandbox2::GetDataDependencyFilePath("");
  std::string project_path = exec_file_path;

  // exec_file_path is /path/to/project/build/exec_file_dir
  // project_path is exec_file_path/../../
  for (size_t i = 0; i != 2; ++i) {
    auto find = project_path.rfind("/");
    if (find == std::string::npos) {
      LOG(ERROR) << kWarning;
      return sandbox2::file::JoinPath(exec_file_path, "test", "images");
    }
    project_path = project_path.substr(0, find);
  }

  return sandbox2::file::JoinPath(project_path, "test", "images", filename);
}

std::string GetFilePath(absl::string_view dir, absl::string_view filename) {
  return sandbox2::file::JoinPath(dir, "test", "images", filename);
}
