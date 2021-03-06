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

#include "check_tag.h"  // NOLINT(build/include)

#include "gtest/gtest.h"
#include "sandboxed_api/util/status_matchers.h"
#include "tiffio.h"  // NOLINT(build/include)

void CheckShortField(TiffApi& api, sapi::v::RemotePtr& tif, const ttag_t field,
                     const uint16_t value) {
  sapi::v::UShort tmp(value + 1);
  absl::StatusOr<int> status_or_int;

  status_or_int = api.TIFFGetField1(&tif, field, tmp.PtrBoth());
  ASSERT_THAT(status_or_int, ::sapi::IsOk()) << "TIFFGetField1 fatal error";
  EXPECT_THAT(status_or_int.value(), ::testing::IsTrue())
      << "Problem fetching tag " << field;
  EXPECT_THAT(tmp.GetValue(), ::testing::Eq(value))
      << "Wrong SHORT value fetched for tag " << field;
}

void CheckShortPairedField(TiffApi& api, sapi::v::RemotePtr& tif,
                           const ttag_t field,
                           const std::array<uint16_t, 2>& values) {
  sapi::v::UShort tmp0(values[0] + 1);
  sapi::v::UShort tmp1(values[1] + 1);
  absl::StatusOr<int> status_or_int;

  status_or_int =
      api.TIFFGetField2(&tif, field, tmp0.PtrBoth(), tmp1.PtrBoth());
  ASSERT_THAT(status_or_int, ::sapi::IsOk()) << "TIFFGetField2 fatal error";
  EXPECT_THAT(status_or_int.value(), ::testing::IsTrue())
      << "Problem fetching tag " << field;
  EXPECT_THAT(tmp0.GetValue(), ::testing::Eq(values[0]))
      << "Wrong SHORT PAIR[0] fetched for tag " << field;
  EXPECT_THAT(tmp1.GetValue(), ::testing::Eq(values[1]))
      << "Wrong SHORT PAIR[1] fetched for tag " << field;
}

void CheckLongField(TiffApi& api, sapi::v::RemotePtr& tif, const ttag_t field,
                    const uint32_t value) {
  sapi::v::UInt tmp(value + 1);
  absl::StatusOr<int> status_or_int;

  status_or_int = api.TIFFGetField1(&tif, field, tmp.PtrBoth());
  ASSERT_THAT(status_or_int, ::sapi::IsOk()) << "TIFFGetField1 fatal error";
  EXPECT_THAT(status_or_int.value(), ::testing::IsTrue())
      << "Problem fetching tag " << field;
  EXPECT_THAT(tmp.GetValue(), ::testing::Eq(value))
      << "Wrong LONG value fetched for tag " << field;
}
