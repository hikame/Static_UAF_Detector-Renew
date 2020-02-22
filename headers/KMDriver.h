/*
 * KMDriver.h
 *
 *  Created on: 2017年4月26日
 *      Author: kame
 */

#ifndef KMDRIVER_H_
#define KMDRIVER_H_

#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Analysis/BasicAliasAnalysis.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/PassAnalysisSupport.h>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "Common.h"

using namespace llvm;

//KM: Added to make the console output more verbose.
#define VERBOSE_SA
#define DEBUG_SA

#define MAX_IS_RECORD 100
// #define Ignored_Func_List_Record "./ignored_function_list.conf"
//
// typedefs
//
typedef std::vector< std::pair<llvm::Module*, llvm::StringRef> > ModuleList;
// Mapping module to its file name.
typedef std::unordered_map<llvm::Module*, llvm::StringRef> ModuleNameMap;
// The set of all functions.
typedef llvm::SmallPtrSet<llvm::Function*, 8> FuncSet;
// Mapping from function name to function.
typedef std::unordered_map<std::string, llvm::Function*> NameFuncMap;
typedef llvm::SmallPtrSet<llvm::CallInst*, 8> CallInstSet;
typedef llvm::DenseMap<llvm::Function*, CallInstSet> CallerMap;
typedef llvm::DenseMap<llvm::CallInst *, FuncSet> CalleeMap;

#ifndef KM_ENUM
#define KM_ENUM

enum ExecuteRelationship{
	Start,
	Call,
	Return,
	Switch_Sure,
	Switch_NotSure,
	Branch_True,
	Branch_False,
	Branch_Undetermined,
	Branch_UnConditional,
	Select_True,
	Select_False,
	MallocResult_Failed,
	MallocResult_Success,
	Fake_Return_Nonnull,
	Fake_Return_Null,
	CMP_True,
	CMP_False,
	Symbolic_Index,
	TODET
};
enum TrackTag{
	Malloc,
	Free,
	Reuse,
	WTF
};
#endif

#define ValueRecord Value* //This can be a Value*, MemoryBlock* or FakeValue*

std::string GetERString(ExecuteRelationship);
std::string GetThreadID();
std::string GetCurrentTime();
extern std::set<ExecuteRelationship> NoPrintSet;
//extern GVC_t* rgContext;

class UafDetectionPass;
class DataLayoutHelper;
class UAFSourceSinkManager;
struct GlobalContext {
	unsigned cpLimit = 0;
  unsigned thdPerCPU = 0;
  unsigned bbAnaLimit = 1;
  unsigned funcAnaLimit = 6;

  DataLayoutHelper* dlHelper = NULL;
  std::set<ExecuteRelationship> LoopCheckSet;
  UafDetectionPass* UDP;
  UAFSourceSinkManager* ssm;
  bool trustGlobalInit = true;
  bool continueAAS = false; //continue analysis after sunk;
  bool printDB = true;
  bool printWN = false;
  bool printER = false;
  bool printTC = false;
  bool shouldQuit = false;
  LLVMContext* llvmContext = NULL;

  std::string outputPath;
  std::mutex sinkRecordsLock;
  std::mutex opLock;
  std::mutex tfLock;
  std::mutex isLock;
  std::vector<std::pair<Value*, std::string>> isMap;

  std::set<std::string> ignoredFuncs;
  void initIgnoredFuncs(std::string path){
  	std::fstream fs;
  	char buf[256];
  	fs.open(path, std::ios::in);
    if(!fs) {
      printf("[ERR] Failed to open %s!!!\n", path.c_str());
      printf("[ERR] Continue without any ignored functions...\n");
      return;
    }
  	while(!fs.eof()){
  		fs.getline(buf, 256);
      if(buf[0] == '#')
        continue;
  		ignoredFuncs.insert(buf);
  	}
  	fs.close();
  }

  std::string GetInstStr(Value* value){
    if(value == NULL)
      return "NULL INST";

		std::lock_guard<std::mutex> lg(isLock);
		for(std::pair<Value*, std::string> pr : isMap){
			if(pr.first == value)
				return pr.second;
		}

		std::string instStr;
		llvm::raw_string_ostream rso(instStr);
		value->print(rso);

		if(isMap.size() > MAX_IS_RECORD)
			isMap.erase(isMap.begin());
		isMap.push_back(std::pair<Value*, std::string>(value, instStr));
		return instStr;
  }

  GlobalContext() {
  	UDP = NULL;
  	ssm = NULL;
  	NumStaticMallocs = 0;
  	NumDynamicMallocs = 0;
  	NumUnsafeMallocs = 0;
  	NumFunctions = 0;
  	NumMallocBytes = 0;
  }

  ~GlobalContext() {
  }

  // Statistics for allocations.
  unsigned NumStaticMallocs;
  unsigned NumDynamicMallocs;
  unsigned NumUnsafeMallocs;
  unsigned NumMallocBytes;
  unsigned NumFunctions;
  std::set<Value *> ValueCounter;

  bool memAvaliable = true;

  // Map global function name to function defination.
  NameFuncMap Funcs;

  // Functions whose addresses are taken.
  FuncSet AddressTakenFuncs;

  // Map a callsite to all potential callee functions.
  CalleeMap Callees;

  // Map a function to all potential caller instructions.
  CallerMap Callers;

  // Indirect call instructions.
  std::vector<CallInst *>IndirectCallInsts;

  // Modules.
  ModuleList Modules;
  ModuleNameMap ModuleMaps;
  std::set<std::string> InvolvedModules;

  // The store target is safe if it is local.
  DenseMap<Function*, SmallPtrSet<Value *, 8>>SafeStoreTargets;
  DenseMap<Function*, SmallPtrSet<Value *, 8>>UnsafeStoreTargets;

  // The signatures of functions that may leak kernel data to user
  // space, stored in file sink.sig.
  std::unordered_map<std::string, std::set<int>> SinkFuncs;
  // Some manaully-verified functions that will not reach sink
  // functions.
  std::set<std::string> NonSinkFuncs;
  // Some manually-summarized functions that initialize
  // values.
  std::map<std::string, std::pair<uint8_t, int8_t>> InitFuncs;
  std::map<std::string, std::tuple<uint8_t, uint8_t, int8_t>> CopyFuncs;
  std::set<std::string> HeapAllocFuncs;
};

class IterativeModulePass {
protected:
  GlobalContext *globalContext;
  const char * ID;
public:
  IterativeModulePass(GlobalContext *Ctx_, const char *ID_)
    : globalContext(Ctx_), ID(ID_) { }

  // Run on each module before iterative pass.
  virtual bool doInitialization(llvm::Module *M)
    { return true; }

  // Run on each module after iterative pass.
  virtual bool doFinalization(llvm::Module *M)
    { return true; }

  // Iterative pass.
  virtual bool doModulePass(llvm::Module *M)
    { return false; }

  virtual void run(ModuleList &modules);

  //~IterativeModulePass(){};
  virtual ~IterativeModulePass(){};
};

#endif /* KMDRIVER_H_ */
