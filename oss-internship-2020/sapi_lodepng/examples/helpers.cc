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

#include "helpers.h"

std::vector<uint8_t> GenerateValues() {
  std::vector<uint8_t> image(kImgLen);
  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      image[4 * kWidth * y + 4 * x + 0] = 255 * !(x & y);
      image[4 * kWidth * y + 4 * x + 1] = x ^ y;
      image[4 * kWidth * y + 4 * x + 2] = x | y;
      image[4 * kWidth * y + 4 * x + 3] = 255;
    }
  }
  return image;
}

std::string CreateTempDirAtCWD() {
  std::string cwd = sandbox2::file_util::fileops::GetCWD();
  CHECK(!cwd.empty());
  cwd.append("/");

  sapi::StatusOr<std::string> result = sandbox2::CreateTempDir(cwd);
  CHECK(result.ok());
  return result.value();
}