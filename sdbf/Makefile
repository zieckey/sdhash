# weizili@360.cn 

CC=gcc
CXX=g++
AR=ar
ARFLAGS=cru
CURRENT_DIR=$(shell echo `pwd`)
SOURCE_DIR=$(CURRENT_DIR)/src
PREFIX=${CURRENT_DIR}/../../install
DEBUG= -g3 
CFLAGS=$(DEBUG) -g -c -fPIC -msse4.2\
	   -fno-strict-aliasing -D_FILE_OFFSET_BITS=64 -D_LARGE_FILE_API -D_BSD_SOURCE -I./external \
	   -Wall \
	   -Wshadow -Wcast-qual -Wcast-align -Wwrite-strings -Wsign-compare \
	   -Winvalid-pch -fms-extensions -Wfloat-equal -Wextra \
	   -I .. -I include -I/usr/local/include  -MMD
CPPFLAGS= $(CFLAGS) -Woverloaded-virtual -Wsign-promo -fno-gnu-keywords
LDFLAGS=-L/usr/local/lib -lpthread  \
		-L .. -L../external/stage/lib \
		-lboost_regex -lboost_system -lboost_filesystem -lboost_program_options \
		-lc -lm -lcrypto -lboost_thread -lpthread


LIBNAME=sdbf

SRCS := $(wildcard *.cc)
OBJS := $(patsubst %.cc, %.o, $(SRCS))
DEPS := $(patsubst %.o, %.d, $(OBJS))

TARGET_SO=lib$(LIBNAME).so
TARGET_A=lib$(LIBNAME).a

all : $(TARGET_SO) $(TARGET_A)
	$(MAKE) install

$(TARGET_A) : $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS) 
	ranlib $@			   

$(TARGET_SO) : $(OBJS)
	$(CXX) $(LDFLAGS)  $(OBJS) -shared -o $@
	
check : $(TARGET_A) $(TARGET_SO)	
	$(MAKE) install
	$(MAKE) check -C test
	$(MAKE) check -C quick_test
	
%.o : %.cc
	$(CXX) $(CPPFLAGS) $< -o $@

-include $(DEPS)

t:
	@echo "LIBNAME:" $(LIBNAME)
	@echo "SRCS:" $(SRCS)
	@echo "OBJS:" $(OBJS)
	@echo "CFLAGS:" $(CFLAGS)
	@echo "CPPFLAGS:" $(CPPFLAGS)

install: $(TARGET_A) $(TARGET_SO) 


clean:
	rm -rf *.o *.d $(OBJS) $(DEPS) $(TARGET_SO) $(TARGET_A)

