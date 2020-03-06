CFLAGS=-g -O0 -emit-llvm -c -S
DEFINE_FLAGS=-DINCLUDEMAIN
CLANG_VER=6.0
LLVM_VER=6.0

C_SOURCES=$(wildcard CWE*.c)
C_SOURCES_A=$(wildcard CWE*a.c)
C_SOURCES_B=$(wildcard CWE*b.c)

NEED_MERGE_PREFIX=$(patsubst %a.c,%,$(C_SOURCES_A))
MERGE_RESULT=$(patsubst %a.c,%.result,$(C_SOURCES_A))

NORMAL_SOURCES=$(filter-out $(C_SOURCES_A) $(C_SOURCES_B), $(C_SOURCES))
NORMAL_RESULT=$(NORMAL_SOURCES:.c=.result)

LL_OBJECTS=$(C_SOURCES:.c=.ll)
BC_OBJECTS=$(LL_OBJECTS:.ll=.bc)

all: $(BC_OBJECTS) 

$(LL_OBJECTS): %.ll:%.c 
	clang-$(CLANG_VER) $(CFLAGS) $(DEFINE_FLAGS) $^ -o $@

$(BC_OBJECTS): %.bc:%.ll
	llvm-as-$(LLVM_VER) $^

$(NORMAL_RESULT):
	mkdir $@
	kmdriver_dbg -bbAnalyzeLimit 2 -cplimit 10 -ctn true -printTC false -printER true -printDB true -printWN true -tm main -suaf -o "$@" $(patsubst %.result,%.bc, $@) 2>&1 | tee "$@/test.log"

$(MERGE_RESULT):
	mkdir $@
	llvm-link-$(LLVM_VER) $(patsubst %.result,%a.bc, $@) $(patsubst %.result,%b.bc, $@) -o $(patsubst %.result, %.bc, $@)
	kmdriver_dbg -bbAnalyzeLimit 2 -cplimit 10 -ctn true -printTC false -printER true -printDB true -printWN true -tm main -suaf -o "$@" $(patsubst %.result, %.bc, $@) 2>&1 | tee "$@/test.log"

build: $(LL_OBJECTS) $(BC_OBJECTS)

normal_test: $(NORMAL_RESULT)

merge_test: $(MERGE_RESULT)

test: build normal_test merge_test

retest: clean test

clean:
	rm -rf *.o *.out *.ll *.bc *.result