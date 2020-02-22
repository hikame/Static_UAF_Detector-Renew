# LLVM_VER=8
# CLANG_VER=8

LLVM_VER=6.0
CLANG_VER=6.0

all: kmdriver debug

./builds/AnalysisState.bc: ./AnalysisState.cc ./headers/*.h
	clang++-$(CLANG_VER) -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -emit-llvm -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./builds/AnalysisState.bc ./AnalysisState.cc 

./builds/ConstantValueWrapper.bc: ./ConstantValueWrapper.cc ./headers/*.h
	clang++-$(CLANG_VER) -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -emit-llvm -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./builds/ConstantValueWrapper.bc ./ConstantValueWrapper.cc 
	 
./builds/UafDetectionPass.bc: ./UafDetectionPass.cc ./headers/*.h
	clang++-$(CLANG_VER) -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -emit-llvm -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./builds/UafDetectionPass.bc ./UafDetectionPass.cc 

./builds/FieldRelationship.bc: ./FieldRelationship.cc ./headers/*.h
	clang++-$(CLANG_VER) -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -emit-llvm -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./builds/FieldRelationship.bc ./FieldRelationship.cc 

./builds/UAFSourceSinkManager.bc: ./UAFSourceSinkManager.cc ./headers/*.h
	clang++-$(CLANG_VER) -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -emit-llvm -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./builds/UAFSourceSinkManager.bc ./UAFSourceSinkManager.cc 

./builds/CallGraph.bc: ./CallGraph.cc ./headers/*.h
	clang++-$(CLANG_VER) -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -emit-llvm -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./builds/CallGraph.bc ./CallGraph.cc 

./builds/SymbolicValue.bc: ./SymbolicValue.cc ./headers/*.h
	clang++-$(CLANG_VER) -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -emit-llvm -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./builds/SymbolicValue.bc ./SymbolicValue.cc 

./builds/DataLayoutHelper.bc: ./DataLayoutHelper.cc ./headers/*.h
	clang++-$(CLANG_VER) -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -emit-llvm -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./builds/DataLayoutHelper.bc ./DataLayoutHelper.cc 

./builds/KMDriver.bc: ./KMDriver.cc ./headers/*.h
	clang++-$(CLANG_VER) -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -emit-llvm -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./builds/KMDriver.bc ./KMDriver.cc 

./builds/FunctionWrapper.bc: ./FunctionWrapper.cc ./headers/*.h
	clang++-$(CLANG_VER) -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -emit-llvm -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./builds/FunctionWrapper.bc ./FunctionWrapper.cc 

./builds/MemoryBlock.bc: ./MemoryBlock.cc ./headers/*.h
	clang++-$(CLANG_VER) -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -emit-llvm -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./builds/MemoryBlock.bc ./MemoryBlock.cc 

./builds/SinkRecord.bc: ./SinkRecord.cc ./headers/*.h
	clang++-$(CLANG_VER) -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -emit-llvm -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./builds/SinkRecord.bc ./SinkRecord.cc 

builds_path:
	mkdir -p ./builds

kmdriver: ./builds ./builds/AnalysisState.bc ./builds/CallGraph.bc ./builds/ConstantValueWrapper.bc ./builds/DataLayoutHelper.bc ./builds/FieldRelationship.bc ./builds/FunctionWrapper.bc ./builds/KMDriver.bc ./builds/MemoryBlock.bc ./builds/SinkRecord.bc ./builds/SymbolicValue.bc ./builds/UAFSourceSinkManager.bc ./builds/UafDetectionPass.bc UafDetectionPass.cc
	clang++-$(CLANG_VER) -lrt -ldl -ltinfo -lpthread -lz -lm -o kmdriver ./builds/*.bc \
	-lLLVMIRReader -lLLVMBitReader -lLLVMPasses -lLLVMAsmParser -lLLVMBinaryFormat -lLLVMCore -lLLVMSupport -lstdc++

debug:
	make -j -f ./makefile_debug

install:
	sudo ln -sf $(shell pwd)/kmdriver /usr/local/bin/

remove:
	sudo rm /usr/local/bin/kmdriver

clean:
	rm -fr kmdriver ./builds/*
	make clean -f ./makefile_debug