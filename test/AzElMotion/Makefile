# AzElMotion Package Test Project Makefile
#
# Creates an executable in this directory called MotionDualAxis

EXEC_NAME := "AzElMotionTest"
INCLUDE_DIR := "../../Linux_Software/sFoundation/inc/inc-pub"
LDLIBS += -lsFoundation20 -lpthread
CC := g++
OPTIMIZATION := -O3
DEBUG_OPTIMIZATION := -O0
CXXFLAGS := -I$(INCLUDE_DIR) $(OPTIMIZATION)
DEBUG_CXXFLAGS := $(CXXFLAGS) -Wall -Wextra -pedantic -g3 $(DEBUG_OPTIMIZATION)
CXXFLAGS += -g -O1 -fsanitize=address -fno-omit-frame-pointer
LDFLAGS += -fsanitize=address

# Specify source files here
ALL_SRC_FILES := $(wildcard *.cpp)
ALL_OBJS := $(patsubst %.cpp,%.o,$(ALL_SRC_FILES))

# Default target
all: AzElMotionTest

# Generic rule to compile a CPP file into an object
%.o: %.cpp %.hpp
	$(CXX) -c $(CXXFLAGS) -o "$@" $<

AzElMotionTest: $(ALL_OBJS)
    ifneq ($(RPATH),)
	$(CC) -L$(RPATH) -Wl,-rpath=$(RPATH) $(LDFLAGS) -o $(EXEC_NAME) $(ALL_OBJS) $(LDLIBS)
    else
	$(CC) $(LDFLAGS) -o $(EXEC_NAME) $(ALL_OBJS) $(LDLIBS)
    endif


# Remove all object files
.PHONY: clean
clean:
	-find . -type f -name "*.o" -delete

# Destroy all build artifacts, including the executable
.PHONY: real_clean
real_clean: clean
	-rm $(EXEC_NAME)
