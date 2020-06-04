VERSION = -std=c++11

PROGRAM = bdchmp
OS := $(shell uname)
HOST := $(shell hostname)

# export OMPI_CXX=/usr/local/bin/g++-8
MPICC = mpic++ # build parallel c++ code
#MPICC = /usr/local/bin/mpic++ # build parallel c++ code

### turn on MPI by default
MPI = 1


### GNU gcc compiler flags
CFLAGS = -O3 -pg # -march=native -Wall
LDFLAGS += -lm

#ifeq ($(OS), Darwin)
#LIBS += -I/usr/include/malloc
#endif
LIBS += -Iinclude

ifeq ($(MPI),1)
CC = $(MPICC)
#CFLAGS += -D_MPI_
endif

CFLAGS += $(VERSION)
LDFLAGS += $(VERSION)

SRCS = $(wildcard src/*.cpp)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.P)

all: $(PROGRAM)

$(PROGRAM):	$(OBJS)
		$(CC) -o $@ $^ $(LDLIBS) $(LDFLAGS)

%.o : %.cpp
	$(CC) -MMD -c -o $@ $< $(LIBS) $(CFLAGS) 
	@cp $*.d $*.P; \
	  sed -e 's/\#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	      -e '/^$$/ d' -e 's/$$/ :/' < $*.d >> $*.P; \
	  rm -f $*.d

### print variable values by command: make print-VARNAME
print-%  : ; @echo $* = $($*)

### copy binary to case-specific folder
flow grad_P perc source calc_magn_replace two-phase:
	mkdir -p $(DESTDIR)/$@/ && cp $(PROGRAM) $(DESTDIR)/$@/

.PHONY: clean
clean:
	@-rm -f src/*.o src/*.P src/*.d

### avoid including DEPS during clean (why create and then just delete them?)
ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS) 
endif

# end of Makefile 

