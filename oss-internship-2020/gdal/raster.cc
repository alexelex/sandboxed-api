#include <gflags/gflags.h>
#include <glog/logging.h>
#include <syscall.h>

#include <fstream>
#include <iostream>

#include "gdal_sapi.sapi.h"
#include "sandboxed_api/sandbox2/util/fileops.h"

class GdalSapiSandbox : public gdalSandbox {
 public:
  std::string filePath;

  GdalSapiSandbox(std::string path)
      : gdalSandbox(), filePath(std::move(path)) {}

  std::unique_ptr<sandbox2::Policy> ModifyPolicy(
      sandbox2::PolicyBuilder*) override {
    return sandbox2::PolicyBuilder()
        .AllowDynamicStartup()
        .AllowRead()
        .AllowSystemMalloc()
        .AllowWrite()
        .AllowExit()
        .AllowStat()
        .AllowOpen()
        .AllowSyscalls({
            __NR_futex,
            __NR_close,
            __NR_recvmsg,
            __NR_getdents64,
            __NR_lseek,
            __NR_getpid,
            __NR_sysinfo,
            __NR_prlimit64,
            __NR_ftruncate,
            __NR_unlink,
        })
        .AddFile(filePath)
        .BuildOrDie();
  }
};

absl::Status GdalMain(std::string filename) {
  // Reading GDALDataset from a (local, specific) file.
  GdalSapiSandbox sandbox(filename);
  SAPI_RETURN_IF_ERROR(sandbox.Init());
  gdalApi api(&sandbox);

  sapi::v::CStr s(filename.data());

  SAPI_RETURN_IF_ERROR(api.GDALAllRegister());
  auto open = api.GDALOpen(s.PtrBoth(), GDALAccess::GA_ReadOnly);

  LOG(INFO) << "Dataset pointer adress: " << open.value() << std::endl;
  sapi::v::RemotePtr ptr_dataset(open.value());

  LOG(INFO) << ptr_dataset.ToString() << std::endl;
  if (!open.value()) {
    return absl::AbortedError("NULL pointer for Dataset.\n");
  }

  // Printing some general information about the dataset.
  auto driver = api.GDALGetDatasetDriver(&ptr_dataset);
  sapi::v::RemotePtr ptr_driver(driver.value());

  auto driver_short_name = api.GDALGetDriverShortName(&ptr_driver);
  auto driver_long_name = api.GDALGetDriverLongName(&ptr_driver);

  sapi::v::RemotePtr ptr_driver_short_name(driver_short_name.value());
  sapi::v::RemotePtr ptr_driver_long_name(driver_long_name.value());

  LOG(INFO) << "Driver short name: "
            << sandbox.GetCString(ptr_driver_short_name).value().c_str();
  LOG(INFO) << "Driver long name: "
            << sandbox.GetCString(ptr_driver_long_name).value().c_str();

  // Checking that GetGeoTransform is valid.
  std::vector<double> adf_geo_transform(6);
  sapi::v::Array<double> adfGeoTransformArray(&adf_geo_transform[0],
                                              adf_geo_transform.size());

  api.GDALGetGeoTransform(&ptr_dataset, adfGeoTransformArray.PtrBoth())
      .IgnoreError();

  LOG(INFO) << "Origin = (" << adf_geo_transform[0] << ", "
            << adf_geo_transform[3] << ")" << std::endl;
  LOG(INFO) << "Pixel Size = (" << adf_geo_transform[0] << ", "
            << adf_geo_transform[3] << ")" << std::endl;

  std::vector<int> n_blockX_size(1);
  std::vector<int> n_blockY_size(1);

  sapi::v::Array<int> nBlockXSizeArray(&n_blockX_size[0], n_blockX_size.size());
  sapi::v::Array<int> nBlockYSizeArray(&n_blockY_size[0], n_blockY_size.size());

  auto band = api.GDALGetRasterBand(&ptr_dataset, 1);
  LOG(INFO) << "Band pointer adress: " << band.value() << std::endl;
  if (!band.value()) {
    return absl::AbortedError("NULL pointer for Band.\n");
  }

  sapi::v::RemotePtr ptr_band(band.value());
  SAPI_RETURN_IF_ERROR(api.GDALGetBlockSize(
      &ptr_band, nBlockXSizeArray.PtrBoth(), nBlockYSizeArray.PtrBoth()));

  LOG(INFO) << "Block = " << n_blockX_size[0] << " x " << n_blockY_size[0]
            << std::endl;

  std::vector<int> b_got_min(1);
  std::vector<int> b_got_max(1);

  sapi::v::Array<int> b_got_min_array(&b_got_min[0], b_got_min.size());
  sapi::v::Array<int> b_got_max_array(&b_got_max[0], b_got_max.size());

  auto nX_size = api.GDALGetRasterBandXSize(&ptr_band);
  auto nY_size = api.GDALGetRasterBandYSize(&ptr_band);

  std::vector<int8_t> raster_data(nX_size.value() * nY_size.value(), -1);
  sapi::v::Array<int8_t> raster_data_array(&raster_data[0], raster_data.size());

  api.GDALRasterIO(&ptr_band, GF_Read, 0, 0, nX_size.value(), nY_size.value(),
                   raster_data_array.PtrBoth(), nX_size.value(),
                   nY_size.value(), GDT_Byte, 0, 0)
      .IgnoreError();

  std::cout << "Raster data info: " << raster_data_array.ToString()
            << std::endl;

  // To print the data content: `std::cout << raster_data_array.GetData() <<
  // std::endl;`

  return absl::OkStatus();
}

int main(int argc, char** argv) {
  // The file to be converted should be specified in the first argument while
  // running the program.
  if (argc < 2) {
    std::cout << "You need to provide a file name: ./raster "
                 "your_tiff_file_absolute_path\n"
                 "Example: ./raster /usr/home/username/file.tiff"
              << std::endl;
    return EXIT_FAILURE;
  }

  std::ifstream aux_file;
  aux_file.open(argv[1]);
  if (!aux_file.is_open()) {
    std::cout << "Your file name is not valid.\nUnable to open the file."
              << std::endl;
    return EXIT_FAILURE;
  }
  std::string filename(argv[1]);

  if (absl::Status status = GdalMain(filename); !status.ok()) {
    LOG(ERROR) << "Initialization failed: " << status.ToString();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
