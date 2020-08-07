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

#include <stdio.h>
#include <stdlib.h>

#include <iostream>

#include "lodepng_sapi.sapi.h"
#include "sandbox.h"
#include "sandboxed_api/util/flag.h"

ABSL_DECLARE_FLAG(string, sandbox2_danger_danger_permit_all);
ABSL_DECLARE_FLAG(string, sandbox2_danger_danger_permit_all_and_log);

// takes a png image (f1), decodes it and ecodes it into f2.
// can be viewed as copying f1 into f2. This function has a basic usage
// of the decode and encode functions.
void decode_and_encode32(SapiLodepngSandbox &sandbox, LodepngApi &api,
                          const std::string &f1, const std::string &f2) {
  sapi::v::UInt width(0), height(0);
  sapi::v::ConstCStr filename1(f1.c_str()), filename2(f2.c_str());

  // in order to pass unsigned char ** to the function, we pass a variable that
  // contains the pointer.
  sapi::v::IntBase<unsigned char *> image(0);

  api.lodepng_decode32_file(image.PtrBoth(), width.PtrBoth(), height.PtrBoth(),
                            filename1.PtrBefore())
      .IgnoreError();

  // after the function is called, we need to access the data stored at the
  // address to which the previous variable points. To do that, we have to
  // transfer the data from the sandbox memory to this process's memory.
  sapi::v::RemotePtr remote_out_ptr(reinterpret_cast<void *>(image.GetValue()));
  sapi::v::Array<char> out_img(width.GetValue() * height.GetValue());
  out_img.SetRemote(remote_out_ptr.GetValue());
  sandbox.TransferFromSandboxee(&out_img).IgnoreError();

  // now the pixels are available at out_img.GetData()

  // when calling the encoding function, we need only an unsigned char *
  // (instead of **) so we can simply use the sapi::v::Array defined before
  // (PtrBefore will give us a pointer).
  api.lodepng_encode32_file(filename2.PtrBefore(), out_img.PtrBefore(),
                            width.GetValue(), height.GetValue())
      .IgnoreError();

  // Since in this function we do not actually used the pixels, we could simply
  // call the encode function with the pointer from before (the remote pointer,
  // since the memory has already been allocated on the sandboxed process).
  // However, most of the use cases of this library require accessing the pixels
  // which is why the solution in which data is transferred around is used.
}

// compares the pixels of the f1 and f2 png files.
bool cmp_images32(SapiLodepngSandbox &sandbox, LodepngApi &api,
                  const std::string &f1, const std::string &f2) {
  std::cout << "COMPARING IMAGES " << basename(f1.c_str()) << " --- "
            << basename(f2.c_str()) << std::endl;

  sapi::v::UInt width1, height1, width2, height2;
  sapi::v::ConstCStr filename1(f1.c_str()), filename2(f2.c_str());
  sapi::v::IntBase<unsigned char *> image1_ptr(0), image2_ptr(0);

  api.lodepng_decode32_file(image1_ptr.PtrBoth(), width1.PtrBoth(),
                            height1.PtrBoth(), filename1.PtrBefore())
      .IgnoreError();

  api.lodepng_decode32_file(image2_ptr.PtrBoth(), width2.PtrBoth(),
                            height2.PtrBoth(), filename2.PtrBefore())
      .IgnoreError();

  sapi::v::RemotePtr remote_out_ptr1(
      reinterpret_cast<void *>(image1_ptr.GetValue()));
  sapi::v::Array<char> pixels1(width1.GetValue() * height1.GetValue());
  pixels1.SetRemote(remote_out_ptr1.GetValue());
  sandbox.TransferFromSandboxee(&pixels1).IgnoreError();

  sapi::v::RemotePtr remote_out_ptr2(
      reinterpret_cast<void *>(image2_ptr.GetValue()));
  sapi::v::Array<char> pixels2(width2.GetValue() * height2.GetValue());
  pixels2.SetRemote(remote_out_ptr2.GetValue());
  sandbox.TransferFromSandboxee(&pixels2).IgnoreError();

  if (width1.GetValue() != width2.GetValue() ||
      height1.GetValue() != height2.GetValue()) {
    std::cerr << "DIMENSIONS DIFFER\n";
    return false;
  }

  for (size_t i = 0; i < width1.GetValue() * height1.GetValue(); ++i) {
    if (pixels1.GetData()[i] != pixels2.GetData()[i]) {
      std::cerr << "PIXELS DIFFER AT i = " << i << std::endl;
      return false;
    }
  }
  return true;
}

// this test simply copies the png from filename1 to filename2 and filename3
// and then decodes those 2 files and compares the pixels. If those pixels are
// equal, then encoding and decoding worked.
void test1(SapiLodepngSandbox &sandbox, LodepngApi &api,
           const std::string &images_path) {
  std::cout << "test1" << std::endl;

  std::string filename1 = images_path + "/test1.png";
  std::string filename2 = images_path + "/out/test1_1out.png";
  std::string filename3 = images_path + "/out/test1_2out.png";

  // const char *filename1 =
  // "/usr/local/google/home/amedar/sapi_lodepng/test_images/test1.png"; const
  // char *filename2 =
  // "/usr/local/google/home/amedar/sapi_lodepng/test_images/out/test1_1out.png";
  // const char *filename3 =
  // "/usr/local/google/home/amedar/sapi_lodepng/test_images/out/test1_2out.png";

  decode_and_encode32(sandbox, api, filename1, filename2);
  decode_and_encode32(sandbox, api, filename1, filename3);

  if (!cmp_images32(sandbox, api, filename1, filename2)) {
    std::cout << "files are different" << std::endl;
  } else {
    std::cout << "files are not different" << std::endl;
  }

  if (!cmp_images32(sandbox, api, filename3, filename2)) {
    std::cout << "files are different" << std::endl;
  } else {
    std::cout << "files are not different" << std::endl;
  }
}

// at the moment there are no arguments when running
int main(int argc, char *argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  if (argc != 2) {
    std::cout << "usage: " << basename(argv[0]) << " images_folder_path"
              << std::endl;
    return 1;
  }

  std::string images_path(argv[1]);

  SapiLodepngSandbox sandbox(images_path);
  sandbox.Init().IgnoreError();

  LodepngApi api(&sandbox);

  test1(sandbox, api, images_path);

  return 0;
}