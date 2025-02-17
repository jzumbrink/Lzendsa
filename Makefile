GXX = g++ --std=c++20
CC = gcc
EVAL_FLAGS = -O3 -DNDEBUG
DEBUG_FLAGS = -O0 -g -DDEBUG
INFO_FLAGS = -DINFOS
INCLUDE_FLAGS = -Iexternal/ips4o/include/ips4o -Iexternal/libsais -Iexternal/ordered/include -Iexternal/rmq/include -Iexternal/word-packing/include -Iinclude/lzendsa -Itest/utils external/libsais/libsais.c external/libsais/libsais64.c

RI_INCLUDE_FLAGS = -I ~/include -Iexternal/adjusted_r_index -L ~/lib # please ensure that sdsl is installed
R_INDEX_END = -lsdsl -lpthread -ldivsufsort -ldivsufsort64
RI_SURPRESS_WARNINGS = -Wno-deprecated-declarations

SPACE_MEASURE_FLAGS = -Iexternal/malloc_count
MALLOC_COUNT_END = -ldl external/malloc_count/malloc_count.o
#
# ===== CLI tools for the Lzendsa-Index =====
#
lzendsa-build:
	$(GXX) -fopenmp $(EVAL_FLAGS) $(SPACE_MEASURE_FLAGS) $(INFO_FLAGS) -o bin/lzendsa-build $(INCLUDE_FLAGS) lzendsa-build.cpp $(MALLOC_COUNT_END)

lzendsa-count:
	$(GXX) -fopenmp $(EVAL_FLAGS) -o bin/lzendsa-count $(INCLUDE_FLAGS) lzendsa-count.cpp

lzendsa-locate:
	$(GXX) -fopenmp $(EVAL_FLAGS) -o bin/lzendsa-locate $(INCLUDE_FLAGS) lzendsa-locate.cpp

lzendsa-ra:
	$(GXX) -fopenmp $(EVAL_FLAGS) -o bin/lzendsa-ra $(INCLUDE_FLAGS) lzendsa-random-access.cpp

# debug versions
dlzendsa-build:
	$(GXX) -fopenmp $(DEBUG_FLAGS) -o bin/dlzendsa-build $(INCLUDE_FLAGS) lzendsa-build.cpp

dlzendsa-count:
	$(GXX) -fopenmp $(DEBUG_FLAGS) -o bin/dlzendsa-count $(INCLUDE_FLAGS) lzendsa-count.cpp

dlzendsa-locate:
	$(GXX) -fopenmp $(DEBUG_FLAGS) -o bin/dlzendsa-locate $(INCLUDE_FLAGS) lzendsa-locate.cpp

#
# ===== CLI tools for the Lzendri-Index =====
#
lzendri-build:
	$(GXX) -fopenmp $(EVAL_FLAGS) $(SPACE_MEASURE_FLAGS) $(INFO_FLAGS) $(RI_SURPRESS_WARNINGS) -o bin/lzendri-build $(INCLUDE_FLAGS) $(RI_INCLUDE_FLAGS) lzendri-build.cpp $(R_INDEX_END) $(MALLOC_COUNT_END)

lzendri-locate:
	$(GXX) -fopenmp $(EVAL_FLAGS) $(SPACE_MEASURE_FLAGS) $(INFO_FLAGS) $(RI_SURPRESS_WARNINGS) -o bin/lzendri-locate $(INCLUDE_FLAGS) $(RI_INCLUDE_FLAGS) lzendri-locate.cpp $(R_INDEX_END) $(MALLOC_COUNT_END)

#
# Compile external malloc cout file
#
mc:
	$(CC) -Wall -Wextra -g -c external/malloc_count/malloc_count.c -o external/malloc_count/malloc_count.o

#
# Additional tools to carry out the measurements
#
create-patterns:
	$(GXX) $(EVAL_FLAGS) -o bin/create-patterns -Iinclude/lzendsa measurements/create-patterns.cpp

create-arbitrary-indices:
	$(GXX) $(EVAL_FLAGS) -o bin/create-arbitrary-indices -Iinclude/lzendsa measurements/create-arbitrary-indices.cpp

zstat:
	$(GXX) $(EVAL_FLAGS) -o bin/zstat $(INCLUDE_FLAGS) measurements/phrase_length.cpp

remove-null:
	$(GXX) $(EVAL_FLAGS) -o bin/remove-null -Iinclude/lzendsa measurements/remove_null.cpp

#
# Tests
#
test-libsais:
	$(GXX) $(DEBUG_FLAGS) -o bin/test-libsais $(INCLUDE_FLAGS) test/test_libsais.cpp

#
# Bundle commands
#
lzendsa:
	make lzendsa-build
	make lzendsa-count
	make lzendsa-locate
	make lzendsa-ra

lzendri:
	make lzendri-build
	make lzendri-locate

all:
	make mc
	make lzendsa
	make lzendri
	make create-patterns
	make create-arbitrary-indices
	make remove-null

.PHONY: lzendsa-build lzendsa-count lzendsa-locate dlzendsa-build dlzendsa-count dlzendsa-locate create-patterns lzendri-build test-libsais zstat remove-null lzendsa lzendri mc lzendsa-ra create-arbitrary-indices