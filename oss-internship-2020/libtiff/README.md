# sapi-libtiff
Copyright 2020 Google LLC.

## Start use
You should make sure the libtiff submodule is cloned.

`git clone --recursive https://github.com/alexelex/sapi-libtiff`

## Usage

#### Build:
```
mkdir -p build && cd build && \
	cmake .. -DSAPI_ROOT=/path/to/sapi_root -DBUILD_SHARED_LIBS=OFF
make -j8
```

#### Example:
You should add `-DTIFF_SAPI_ENABLE_EXAMPLES=ON` to use the example.\
run: `./example/sandboxed /absolute/path/to/project/dir`

#### Tests:
You should add `-DTIFF_SAPI_ENABLE_TESTS=ON` to use tests.\
run: `./test/tests`
