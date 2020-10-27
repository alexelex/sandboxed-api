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

#include <linux/futex.h>
#include <syscall.h>

#include <utility>
#include <vector>

#include "libpng_sapi.sapi.h"

namespace {

class LibPNGSapiSandbox : public LibPNGSandbox {
 public:
  void AddFile(const std::string& file) { files_.push_back(file); }

 private:
  std::unique_ptr<sandbox2::Policy> ModifyPolicy(
      sandbox2::PolicyBuilder*) override {
    auto builder = std::make_unique<sandbox2::PolicyBuilder>();
    (*builder)
        .AllowRead()
        .AllowStaticStartup()
        .AllowWrite()
        .AllowOpen()
        .AllowExit()
        .AllowStat()
        .AllowMmap()
        .AllowSystemMalloc()
        .AllowSyscalls({
            __NR_futex,
            __NR_close,
            __NR_lseek,
            __NR_gettid,
            __NR_sysinfo,
            __NR_munmap,
            __NR_recvmsg,
            __NR_fcntl,
        });

    for (const auto& file : files_) {
      builder->AddFile(file, /*is_ro=*/false);
    }

    return builder.get()->BuildOrDie();
  }

  std::vector<std::string> files_;
};

}  // namespace
