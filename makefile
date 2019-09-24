#############################################################################
# CASPIAN - A Programmable Neuromorphic Computing Architecture
# Parker Mitchell, 2019
#############################################################################

# Applications
SH ?= bash
CC ?= gcc
CXX ?= g++
AR ?= ar
RANLIB ?= ranlib
PYTHON ?= python3
PIP ?= $(PYTHON) -m pip
VALGRIND ?= valgrind

# Directories
BIN = bin
DOCS = docs
INC = include
LIB = lib
OBJ = obj
SRC = src
EO = eo
UTILS = utils
TST = test
PYBINDINGS = bindings
PYBUILD = build
PYBUILD_BINDINGS = $(PYBUILD)/bindings
PYBUILD_SUFFIX := $(shell python3-config --extension-suffix)

# Framework Directories
ROOT          = ..
ROOT_INCLUDE  = $(ROOT)/include

MKFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
PROJECT_DIR := $(dir $(MKFILE_PATH))

# Compilation Flags
CFLAGS ?= -Wall -Wextra -pipe -O3
CFLAGSBASE = $(CFLAGS) -std=c++11 -I$(INC) -I$(ROOT_INCLUDE)
LFLAGS = -lpthread

## Targets
.PHONY: all clean test run_test python utils
all: $(LIB)/libcaspian.a
 
clean:
	rm -rf $(BIN) $(OBJ) $(LIB) $(PYBUILD)

#########################
## Caspian library
LIBCASPIAN = $(LIB)/libcaspian.a
LIBFRAMEWORK = $(ROOT)/lib/libframework.a
PYLIBCASPIAN = $(PYBUILD)/caspian$(PYBUILD_SUFFIX)
PYFRAMEWORK  = $(ROOT)/build/neuro$(PYBUILD_SUFFIX)

$(LIBFRAMEWORK): $(ROOT_INCLUDE)/framework.hpp
	$(MAKE) -C $(ROOT)

$(PYFRAMEWORK):
	$(MAKE) -C $(ROOT) python

#########################
## Directories
$(BIN) $(OBJ) $(LIB) $(PYBUILD) $(PYBUILD_BINDINGS):
	mkdir -p $@

#########################
## Sources
HEADERS     = $(INC)/backend.hpp \
              $(INC)/constants.hpp \
	      $(INC)/network.hpp \
	      $(INC)/simulator.hpp \
	      $(INC)/spike_encoder.hpp \
	      $(INC)/ucaspian.hpp

TL_HEADERS  = $(INC)/processor.hpp \
              $(INC)/network_conversion.hpp

SOURCES     = $(SRC)/backend.cpp \
	      $(SRC)/network.cpp \
	      $(SRC)/simulator.cpp \
	      $(SRC)/spike_encoder.cpp \
	      $(SRC)/ucaspian.cpp

TL_SOURCES  = $(SRC)/processor.cpp \
              $(SRC)/network_conversion.cpp

OBJECTS    := $(patsubst $(SRC)/%.cpp,$(OBJ)/%.o,$(SOURCES))
TL_OBJECTS := $(patsubst $(SRC)/%.cpp,$(OBJ)/%.o,$(TL_SOURCES))

$(OBJECTS): $(OBJ)/%.o : $(SRC)/%.cpp $(HEADERS) | $(OBJ)
	$(CXX) $(CFLAGSBASE) -c $< -o $@

$(TL_OBJECTS): $(OBJ)/%.o : $(SRC)/%.cpp $(HEADERS) $(TL_HEADERS) $(ROOT_INCLUDE)/framework.hpp | $(OBJ)
	$(CXX) $(CFLAGSBASE) -c $< -o $@

$(LIBCASPIAN): $(OBJECTS) $(TL_OBJECTS) | $(LIB)
	$(AR) r $@ $^
	$(RANLIB) $@

#########################
## Python support

python: $(PYLIBCASPIAN)

PYBUILD_FLAGS := $(shell python3 -m pybind11 --includes) -I$(INC) -I$(ROOT_INCLUDE) -I$(PYBINDINGS) -I$(ROOT)/$(PYBINDINGS) \
                 -std=c++14 -fPIC -O3 -fvisibility=hidden

# Patch symbol linkage issues for Mac OS
OS := $(shell uname)
ifeq ($(OS),"Darwin")
    PYBUILD_FLAGS += -undefined dynamic_lookup -flto
else
    PYBUILD_FLAGS += -flto=8
endif


BINDING_SOURCES := $(PYBINDINGS)/backend.cpp \
                   $(PYBINDINGS)/network.cpp \
		   $(PYBINDINGS)/bindings.cpp \
		   $(PYBINDINGS)/processor.cpp

BINDING_OBJECTS := $(patsubst $(PYBINDINGS)/%.cpp,$(PYBUILD_BINDINGS)/%.o,$(BINDING_SOURCES))

PYBUILD_OBJECTS := $(patsubst $(SRC)/%.cpp,$(PYBUILD)/%.o,$(SOURCES)) \
		   $(patsubst $(SRC)/%.cpp,$(PYBUILD)/%.o,$(TL_SOURCES))

PYBUILD_TL_OBJECTS := $(wildcard $(ROOT)/$(PYBUILD)/*.o)

$(BINDING_OBJECTS): $(PYBUILD_BINDINGS)/%.o : $(PYBINDINGS)/%.cpp $(HEADERS) $(TL_HEADERS) $(ROOT_INCLUDE)/framework.hpp | $(PYBUILD_BINDINGS)
	$(CXX) $(PYBUILD_FLAGS) -c $< -o $@

$(PYBUILD_OBJECTS): $(PYBUILD)/%.o : $(SRC)/%.cpp $(HEADERS) $(TL_HEADERS) $(ROOT_INCLUDE)/framework.hpp | $(PYBUILD)
	$(CXX) $(PYBUILD_FLAGS) -c $< -o $@

$(PYLIBCASPIAN): $(PYBUILD_OBJECTS) $(BINDING_OBJECTS) $(PYFRAMEWORK)
	$(CXX) $(PYBUILD_FLAGS) -shared $(PYBUILD_OBJECTS) $(BINDING_OBJECTS) $(PYBUILD_TL_OBJECTS) -o $@

#########################
## Testing
TEST_SRC  := $(wildcard $(TST)/*.cpp)
TEST_OBJ  := $(patsubst $(TST)/%.cpp,$(OBJ)/%.o,$(TEST_SRC))
TEST_EXEC  = $(BIN)/test

$(TEST_OBJ): $(OBJ)/%.o : $(TST)/%.cpp $(HEADERS) $(TL_HEADERS) | $(OBJ)
	$(CXX) $(CFLAGSBASE) -c $< -o $@

$(TEST_EXEC): $(TEST_OBJ) $(LIBCASPIAN) $(LIBFRAMEWORK) | $(BIN)
	$(CXX) $(CFLAGSBASE) $(TEST_OBJ) -o $(TEST_EXEC) $(LIBCASPIAN) $(LIBFRAMEWORK)

test: $(TEST_EXEC)

run_test: $(TEST_EXEC)
	$(VALGRIND) $(TEST_EXEC)

#########################
## Utilities
UTILITIES  = $(BIN)/benchmark \
             $(BIN)/all_to_all_bench \
             $(BIN)/prune

$(UTILITIES): $(BIN)/% : $(UTILS)/%.cpp $(LIBCASPIAN) $(LIBFRAMEWORK) | $(BIN)
	$(CXX) $(CFLAGSBASE) $< -o $@ $(LIBCASPIAN) $(LIBFRAMEWORK)

utils: $(UTILITIES)

