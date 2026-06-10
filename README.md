# libuv CMake Demo

This project contains small C++17 programs that exercise libuv APIs. By
default, it links against the libuv install prefix at:

```sh
/home/gaoming/projects/libuv/build/install
```

Build and run:

To use a different libuv install prefix:

```sh
cmake -S . -B build -DLIBUV_INSTALL_DIR=/path/to/libuv/install
```
