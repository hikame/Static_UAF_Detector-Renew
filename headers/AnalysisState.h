#ifndef HEADERS_ANALYSISSTATE_H_
#define HEADERS_ANALYSISSTATE_H_

#include "AnalysisStructHeaders.h"
#include "KMDriver.h"

struct CallRecord{
	std::mutex lock;
	Function* func;
	CallInst* callInst;
	// std::set<Value*> localVars;
	CallRecord(Function* f,	CallInst* c){
		func = f;
		callInst = c;
	}
};

struct ExecutionRecord{
	BasicBlock* bb;
	ExecuteRelationship er;
	std::set<TrackTag> tag;
	ExecutionRecord(BasicBlock* b, ExecuteRelationship e){
		bb = b;
		er = e;
	}
};

struct CMPRecord{
	std::shared_ptr<StoredElement> vr1;
	std::shared_ptr<StoredElement> vr2;
	CmpInst::Predicate pred;
	bool result;

	CMPRecord(std::shared_ptr<StoredElement> v1, std::shared_ptr<StoredElement> v2, CmpInst::Predicate pd, bool r);
	~CMPRecord();
};

struct MemoryBlockTag{
	std::string name;
	void* value;
	MemoryBlockTag(std::string n, void* v):name(n), value(v){};
};

class AnalysisState{
private:
	std::map<Value*, std::shared_ptr<StoredElement>> variableValues; //some variables are not pointers for memory block. their value are some value such as const value.
	std::map<std::shared_ptr<MemoryBlock>, std::shared_ptr<StoredElement>> mbContainedValues;
	std::set<std::shared_ptr<CMPRecord>> cmpRecords;
	std::multimap<std::shared_ptr<MemoryBlock>, std::shared_ptr<MemoryBlockTag>> mbTags;
public:
	GlobalContext *globalContext;
	LLVMContext* llvmContext;
	uint64_t instCount = 0;
	bool hasError = false;

	std::list<std::shared_ptr<ExecutionRecord>> executionPath;
	std::vector<std::shared_ptr<CallRecord>> callPath;
	std::multiset<Function*> analyzedFunctions;

	AnalysisState(LLVMContext* lc, GlobalContext *gc){
		llvmContext = lc;
		globalContext = gc;
	};

	// void GenerateArgMemBlock(Function*, std::shared_ptr<CallRecord>);
	void GenerateStackMB(AllocaInst*);

	bool ShouldNotContinue(BasicBlock*);
	bool IsLastAnalyzedBB(BasicBlock* bb);
	void AddAnalyzedBasicBlock(BasicBlock*, ExecuteRelationship);
	std::shared_ptr<CallRecord> AddNewCallRecord(CallInst*, Function*);
	bool ShouldAnalyzeFunction(Function* func);
	CallInst* TakeOutLastCall();
	std::shared_ptr<CallRecord> GetLastCall();
	std::shared_ptr<AnalysisState> MakeCopy();
	void MergeFakeValueRecord(std::shared_ptr<StoredElement> variable, std::shared_ptr<StoredElement> value);
	void PrintExectutionPath();
	void PrepareForStep2();
	void ClearUselessMBTags();
	~AnalysisState();

	/** if there is no record, this function will generate a symbolic value for it*/
	std::shared_ptr<StoredElement> GetVariableRecord(Value*);
	std::shared_ptr<StoredElement> QueryVariableRecord(Value* variable);

	void AddVariableRecord(Value*, std::shared_ptr<StoredElement>);
	void RecordMBContainedValue(std::shared_ptr<MemoryBlock>, 
		std::shared_ptr<StoredElement>);
	std::shared_ptr<StoredElement> GetMBContainedValue(std::shared_ptr<MemoryBlock>, bool autoGenerateFV = true);
	void RecordCMPResult(std::shared_ptr<StoredElement> v1, std::shared_ptr<StoredElement> v2, CmpInst::Predicate pred, bool result);
	Value* QueryCMPRecord(std::shared_ptr<StoredElement> v1, std::shared_ptr<StoredElement> v2, CmpInst::Predicate pred);
	/** Currently, the type of value is only LLVM instruction objects*/
	void AddMemoryBlockTag(std::shared_ptr<MemoryBlock> mb, std::string name, void* value);
	void* GetMemoryBlockTag(std::shared_ptr<MemoryBlock> mb, std::string name);
	void CopyMemoryBlockTags(std::shared_ptr<MemoryBlock> destMB, MemoryBlock* sourceMB);
};

#endif
