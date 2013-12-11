# weizili@360.cn 

CC=gcc
CXX=g++
AR=ar
ARFLAGS=cru
CFLAGS = -g -O3 -fPIC -c -msse4.2 -fno-strict-aliasing -D_FILE_OFFSET_BITS=64 -D_LARGE_FILE_API -D_BSD_SOURCE -I./external -MMD
LDFLAGS = -L . -L./external/stage/lib -lboost_regex -lboost_system -lboost_filesystem -lboost_program_options -lc -lm -lcrypto -lboost_thread -lpthread

TARGET=sdhash

SRCS := $(wildcard sdhash-src/*.cc) $(wildcard sdbf/*.cc) $(wildcard base64/*.cc) $(wildcard lz4/*.cc)
OBJS := $(patsubst %.cc, %.o, $(SRCS))
DEPS := $(patsubst %.o, %.d, $(OBJS))

all : $(TARGET)
	
$(TARGET) : $(OBJS)
	g++ $(OBJS) $(LDFLAGS) -o sdhash

init : 
	$(MAKE) -f Makefile.original
	
%.o : %.cc
	$(CXX) $(CFLAGS) $< -o $@

-include $(DEPS)

clean:
	rm -rf *.o *.d $(OBJS) $(DEPS) $(TARGET_SO) $(TARGET_A)

