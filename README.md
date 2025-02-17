# Lzendsa
LZ-End compressed suffix array implementation.

# Disclaimer

The construction of the LZ-End parsing implemented in `include/lzendsa/integer_lzend.hpp` is based on Dinklages implementation for the construction of the LZ-End-parsing (which can be found [here](https://github.com/pdinklag/lzend)) and is adjusted to work for integer arrays.

## Installation

Make sure [sdsl](https://github.com/simongog/sdsl-lite) is installed.

Install all git submodules with `git submodule update --recursive --init`. 

Install `tbb` via `sudo apt install libtbb-dev` (if using linux) in order for ips4o to work.

To build the project you can use `make all`. 
For other `make` commands you have to build the malloc_count object file once via `make mc` first.
To build specific parts of this project you can use `make lzendsa` to build the cli tools for the lzendsa-index and `make lzendri` to build the cli tools for the lzendri-index.
Or you can use the make commands for each cli tool `lzendsa-build`, `lzendsa-count`, `lzendsa-locate`, `lzendsa-ra`, `lzendri-build` or `lzendri-locate`.

## Run

### Lzendsa

To build the lzendsa-index of an input file, run

> ```./bin/lzendsa-build -o idx_location input```

which will build the lzendsa-index for the file `input` and stores the index in `idx_location.lzendsa`.
To count the occurences of a pattern, run
> ```./bin/lzendsa-count idx_location.lzendsa input patterns```

where `patterns` is the part to the file that contains the patterns in the Pizza&Chili format.
To locate all occurences of a pattern, run

>```./bin/lzendsa-locate idx_location.lzendsa input patterns```

### Lzendri

To build the lzendri-index of an input file, run

> ```./bin/lzendri-build -o idx_location input```

which will build the lzendri-index for the file `input` and stores the index in `idx_location.lzendri`.
To locate all occurences of a pattern, run

>```./bin/lzendri-locate idx_location.lzendsa patterns```