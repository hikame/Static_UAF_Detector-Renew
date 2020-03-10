BCS=$(SRCS:.c=.bc)
#CLANG_VER=8
#LLVM_VER=8
CLANG_VER=6.0
LLVM_VER=6.0

BUILD_FLAG = -pie -fPIC -g -O0 -Wuninitialized 
ifeq ($(TARGET), )
	TARGET = main
endif

all: test.bin merged.bc dis

$(BCS): $(SRCS)
	clang-$(CLANG_VER) $(BUILD_FLAG) -emit-llvm -c $^ 

merged.bc: $(BCS)
	llvm-link-$(LLVM_VER) $^ -o $@ 

dis : merged.bc
	llvm-dis-$(LLVM_VER) merged.bc

test.bin: merged.bc
	clang-$(CLANG_VER) $(BUILD_FLAG) $^ -o $@

run:
	./test.bin

show:
	kmdrawer -e "$(PWD)" ./
	kmdrawer -s -e "$(PWD)" ./
	code ./*_normal.png ./*.c

test: merged.bc
	kmdriver_dbg -bbAnalyzeLimit 2 -cplimit 10 -ctn false -printTC true -printER true -printDB true -printWN true -tm $(TARGET) -suaf -o "$(PWD)" "$(PWD)/merged.bc"

retest: clean test dis show 

clean:
	@rm -rf *.bin *.bc *.ll *.kmd *.png

.PHONY: all run clean test
