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

#ifndef GDAL_SANDBOX_H_
#define GDAL_SANDBOX_H_

#include <string>

#include <syscall.h>

#include "gdal_sapi.sapi.h"
#include "sandboxed_api/sandbox2/util/fileops.h"

namespace gdal::sandbox {

class GdalSapiSandbox : public gdalSandbox {
 public:
  GdalSapiSandbox(std::string path) 
    : gdalSandbox(),
      path_(std::move(path)) 
    {}

  std::unique_ptr<sandbox2::Policy> ModifyPolicy(
      sandbox2::PolicyBuilder*) override {

      return sandbox2::PolicyBuilder()
        .AllowDynamicStartup()
        .AllowRead()
        .AllowSystemMalloc()
        .AllowWrite()
        .AllowExit()
        .AllowOpen()
        .AllowSyscalls({
            __NR_futex,
            __NR_getdents64,  // DriverRegisterAll()
            __NR_lseek,  // GDALCreate()
            __NR_getpid,  // GDALCreate()
            __NR_sysinfo,  // VSI_TIFFOpen_common()
            __NR_prlimit64,  // CPLGetUsablePhysicalRAM()
            __NR_ftruncate,  // GTiffDataset::FillEmptyTiles()
            __NR_unlink,  // GDALDriver::Delete()
        })
        // TODO: Deal with proj.db so you don't need to specify exact path ih the policy
        .AddFile("/usr/local/share/proj/proj.db")  // proj.db is required
        .AddDirectory("/usr/local/lib")  // To add libproj.so.19.1.1
        .AddDirectory(path_, /*is_ro=*/false)
        .BuildOrDie();
    }

 private:
  std::string path_;
};

}  // namespace gdal::sandbox

#endif  // GDAL_SANDBOX_H_
