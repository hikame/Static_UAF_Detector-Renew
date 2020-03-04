LEVEL = ..
SRCS = test.c
include $(LEVEL)/Makefile.inc

test2.bc: test2.c
	clang-$(CLANG_VER) $(BUILD_FLAG) -emit-llvm -c test2.c

dis2 : test2.bc
	llvm-dis-$(LLVM_VER) test2.bc

test2: test2.bc
	kmdriver_dbg -bbAnalyzeLimit 2 -cplimit 10 -ctn false -printTC true -printER true -printDB true -printWN true -tm foo0 -suaf -o "$(PWD)" "$(PWD)/test2.bc"
	kmdriver_dbg -bbAnalyzeLimit 2 -cplimit 10 -ctn false -printTC true -printER true -printDB true -printWN true -tm foo1 -suaf -o "$(PWD)" "$(PWD)/test2.bc"

retest: clean test2 test dis dis2