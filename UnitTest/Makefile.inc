	BCS=$(SRCS:.c=.bc)
#CLANG_VER=8
#LLVM_VER=8
CLANG_VER=6.0
LLVM_VER=6.0


# BUILD_FLAG = -g -O0
BUILD_FLAG = -pie -fPIC -g -O0 -Wuninitialized 

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
	code ./*_normal.png

test: merged.bc
	kmdriver -bbAnalyzeLimit 2 -cplimit 10 -ctn false -printTC true -printER true -printDB true -printWN true -tm main -suaf -o "$(PWD)" -ifs /home/kame/workspace/Static_UAF_Detector/KMDriver_v5/ignored_function_list.conf "$(PWD)/merged.bc"
	kmdrawer -e "$(PWD)" ./
	kmdrawer -s -e "$(PWD)" ./

retest: clean test dis show 

help:
	echo "kmdriver -cplimit 10 -ctn false -printER true -printDB true -printEP true -printWN true -tm main -suaf -o \"$(PWD)\" -ifs /home/kame/workspace/Static_UAF_Detector/KMDriver_v5/ignored_function_list.conf \"$(PWD)/merged.bc\""

clean:
	@rm -rf *.bin *.bc *.ll *.kmd *.png

.PHONY: all run clean test
