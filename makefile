LLVM_VER=6.0
CLANG_VER=6.0

all: path release debug

path: 
	test -d builds || mkdir -p builds
	test -d dbg_builds || mkdir -p dbg_builds

release: ./builds/kmdriver

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

# ./builds/CallGraph.bc: ./CallGraph.cc ./headers/*.h
# 	clang++-$(CLANG_VER) -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -emit-llvm -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
# 	./builds/CallGraph.bc ./CallGraph.cc 

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

./builds/kmdriver: ./builds/AnalysisState.bc ./builds/ConstantValueWrapper.bc ./builds/DataLayoutHelper.bc ./builds/FieldRelationship.bc ./builds/FunctionWrapper.bc ./builds/KMDriver.bc ./builds/MemoryBlock.bc ./builds/SinkRecord.bc ./builds/SymbolicValue.bc ./builds/UAFSourceSinkManager.bc ./builds/UafDetectionPass.bc
	clang++-$(CLANG_VER) -lrt -ldl -ltinfo -lpthread -lz -lm -o ./builds/kmdriver \
	./builds/*.bc -lLLVMIRReader -lLLVMBitReader -lLLVMPasses -lLLVMAsmParser -lLLVMBinaryFormat -lLLVMCore -lLLVMSupport -lstdc++

debug: ./dbg_builds/kmdriver_dbg

./dbg_builds/AnalysisState.bc: ./AnalysisState.cc ./headers/*.h
	clang++-$(CLANG_VER) -D_GLIBCXX_DEBUG -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -O0 -emit-llvm -g3 -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./dbg_builds/AnalysisState.bc ./AnalysisState.cc 

./dbg_builds/ConstantValueWrapper.bc: ./ConstantValueWrapper.cc ./headers/*.h
	clang++-$(CLANG_VER) -D_GLIBCXX_DEBUG -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -O0 -emit-llvm -g3 -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./dbg_builds/ConstantValueWrapper.bc ./ConstantValueWrapper.cc 
	 
./dbg_builds/UafDetectionPass.bc: ./UafDetectionPass.cc ./headers/*.h
	clang++-$(CLANG_VER) -D_GLIBCXX_DEBUG -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -O0 -emit-llvm -g3 -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./dbg_builds/UafDetectionPass.bc ./UafDetectionPass.cc 

./dbg_builds/FieldRelationship.bc: ./FieldRelationship.cc ./headers/*.h
	clang++-$(CLANG_VER) -D_GLIBCXX_DEBUG -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -O0 -emit-llvm -g3 -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./dbg_builds/FieldRelationship.bc ./FieldRelationship.cc 

./dbg_builds/UAFSourceSinkManager.bc: ./UAFSourceSinkManager.cc ./headers/*.h
	clang++-$(CLANG_VER) -D_GLIBCXX_DEBUG -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -O0 -emit-llvm -g3 -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./dbg_builds/UAFSourceSinkManager.bc ./UAFSourceSinkManager.cc 

# ./dbg_builds/CallGraph.bc: ./CallGraph.cc ./headers/*.h
# 	clang++-$(CLANG_VER) -D_GLIBCXX_DEBUG -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -O0 -emit-llvm -g3 -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
# 	./dbg_builds/CallGraph.bc ./CallGraph.cc 

./dbg_builds/SymbolicValue.bc: ./SymbolicValue.cc ./headers/*.h
	clang++-$(CLANG_VER) -D_GLIBCXX_DEBUG -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -O0 -emit-llvm -g3 -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./dbg_builds/SymbolicValue.bc ./SymbolicValue.cc 

./dbg_builds/DataLayoutHelper.bc: ./DataLayoutHelper.cc ./headers/*.h
	clang++-$(CLANG_VER) -D_GLIBCXX_DEBUG -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -O0 -emit-llvm -g3 -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./dbg_builds/DataLayoutHelper.bc ./DataLayoutHelper.cc 

./dbg_builds/KMDriver.bc: ./KMDriver.cc ./headers/*.h
	clang++-$(CLANG_VER) -D_GLIBCXX_DEBUG -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -O0 -emit-llvm -g3 -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./dbg_builds/KMDriver.bc ./KMDriver.cc 

./dbg_builds/FunctionWrapper.bc: ./FunctionWrapper.cc ./headers/*.h
	clang++-$(CLANG_VER) -D_GLIBCXX_DEBUG -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -O0 -emit-llvm -g3 -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./dbg_builds/FunctionWrapper.bc ./FunctionWrapper.cc 

./dbg_builds/MemoryBlock.bc: ./MemoryBlock.cc ./headers/*.h
	clang++-$(CLANG_VER) -D_GLIBCXX_DEBUG -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -O0 -emit-llvm -g3 -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./dbg_builds/MemoryBlock.bc ./MemoryBlock.cc 

./dbg_builds/SinkRecord.bc: ./SinkRecord.cc ./headers/*.h
	clang++-$(CLANG_VER) -D_GLIBCXX_DEBUG -I/usr/include/llvm-$(LLVM_VER) -I/usr/include/llvm-c-$(LLVM_VER) -O0 -emit-llvm -g3 -Wall -pthread -c -fmessage-length=0 -fPIC -std=c++0x -o \
	./dbg_builds/SinkRecord.bc ./SinkRecord.cc 

./dbg_builds/kmdriver_dbg: ./dbg_builds/AnalysisState.bc ./dbg_builds/ConstantValueWrapper.bc ./dbg_builds/DataLayoutHelper.bc ./dbg_builds/FieldRelationship.bc ./dbg_builds/FunctionWrapper.bc ./dbg_builds/KMDriver.bc ./dbg_builds/MemoryBlock.bc ./dbg_builds/SinkRecord.bc ./dbg_builds/SymbolicValue.bc ./dbg_builds/UAFSourceSinkManager.bc ./dbg_builds/UafDetectionPass.bc
	clang++-$(CLANG_VER) -O0 -g3 -D_GLIBCXX_DEBUG -lrt -ldl -ltinfo -lpthread -lz -lm -o ./dbg_builds/kmdriver_dbg \
	./dbg_builds/*.bc -lLLVMIRReader -lLLVMBitReader -lLLVMPasses -lLLVMAsmParser -lLLVMBinaryFormat -lLLVMCore -lLLVMSupport -lstdc++

install: debug release
	sudo ln -sf $(shell pwd)/builds/kmdriver /usr/local/bin/
	sudo ln -sf $(shell pwd)/dbg_builds/kmdriver_dbg /usr/local/bin/

remove:
	sudo rm /usr/local/bin/kmdriver
	sudo rm /usr/local/bin/kmdriver_dbg

clean:
	rm -fr ./builds ./dbg_builds