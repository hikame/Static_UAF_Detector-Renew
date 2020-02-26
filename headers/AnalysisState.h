#ifndef HEADERS_ANALYSISSTATE_H_
#define HEADERS_ANALYSISSTATE_H_

#include "AnalysisStructHeaders.h"
#include "KMDriver.h"

enum WarningType{
	Call_Inline_Asm_Function,
	Call_without_Known_Target_Function,
	Call_Target_has_no_Body,
	Call_Depth_Exceed_Limit,

	GEP_Failed,
	GEP_with_Symbolic_Index,
	GEP_with_Symbolic_Index_from_NonArrayType,
	GEP_with_Negative_Index,
	GEP_with_Strange_Index,
	GEP_from_Pointer,
	GEP_from_Strange_Type,
	GEP_using_Strange_Size,
	GEP_without_Suitable_Container,

	Memcpy_with_Symbolic_Size,
	Memcpy_without_Suitable_Source,
	Memcpy_without_Suitable_Dest,
	Memcpy_with_Mismatched_Source_Dest,
	Memmove_with_Symbolic_Size,
	Memset_with_Strange_Target,
	Memset_with_Symbolic_Size,
	Memset_with_Strange_Field,

	Global_without_Initializer,
	Load_from_Larger_MemoryBlock,
	Store_to_Larger_MemoryBlock,
	Return_without_Value_Record
};

struct CallRecord{
	std::mutex lock;
	Function* func;
	CallInst* callInst;
	std::map<Value*, std::shared_ptr<StoredElement>> localValueRecord; //some variables are not pointers for memory block. their value are some value such as const value.
	
	CallRecord(Function* f,	CallInst* c){
		func = f;
		callInst = c;
	}

	CallRecord(std::shared_ptr<CallRecord> source){  //copy
		func = source->func;
		callInst = source->callInst;
		for(auto pair : source->localValueRecord)
			localValueRecord[pair.first] = pair.second;
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
	std::map<Value*, std::shared_ptr<StoredElement>> globalValueRecord; //some variables are not pointers for memory block. their value are some value such as const value.
	std::map<std::shared_ptr<MemoryBlock>, std::shared_ptr<StoredElement>> mbContainedValues;
	std::set<std::shared_ptr<CMPRecord>> cmpRecords;
	std::multimap<std::shared_ptr<MemoryBlock>, std::shared_ptr<MemoryBlockTag>> mbTags;
public:
	GlobalContext *globalContext;
	LLVMContext* llvmContext;
	uint64_t instCount = 0;
	bool hasError = false;

	std::list<std::shared_ptr<ExecutionRecord>> executionPath;
	std::multimap<BasicBlock*, WarningType> warnRecords;
	std::vector<std::shared_ptr<CallRecord>> callPath;
	std::multiset<Function*> analyzedFunctions;

	AnalysisState(LLVMContext* lc, GlobalContext *gc){
		llvmContext = lc;
		globalContext = gc;
	};

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
	void PrintExectutionPath(bool completePath = false, bool needConsoleLock = false);
	void PrepareForStep2();
	void ClearUselessMBTags();
	~AnalysisState();

	/* if there is no record, this function will generate a symbolic value for it
	 * level is only for record query of local variables
	*/
	std::shared_ptr<StoredElement> GetVariableRecord(Value*, int level = 0);
	std::shared_ptr<StoredElement> QueryVariableRecord(Value* variable, int level = 0);

	void AddLocalVariableRecord(Value*, std::shared_ptr<StoredElement>, int level = 0);
	void AddGlobalVariableRecord(Value*, std::shared_ptr<StoredElement>);
	void RecordMBContainedValue(std::shared_ptr<MemoryBlock>, 
		std::shared_ptr<StoredElement>);
	std::shared_ptr<StoredElement> GetMBContainedValue(std::shared_ptr<MemoryBlock>, bool autoGenerateFV = true);
	void RecordCMPResult(std::shared_ptr<StoredElement> v1, std::shared_ptr<StoredElement> v2, CmpInst::Predicate pred, bool result);
	Value* QueryCMPRecord(std::shared_ptr<StoredElement> v1, std::shared_ptr<StoredElement> v2, CmpInst::Predicate pred);
	/** Currently, the type of value is only LLVM instruction objects*/
	void AddMemoryBlockTag(std::shared_ptr<MemoryBlock> mb, std::string name, void* value);
	void* GetMemoryBlockTag(std::shared_ptr<MemoryBlock> mb, std::string name);
	void CopyMemoryBlockTags(std::shared_ptr<MemoryBlock> destMB, MemoryBlock* sourceMB);
	bool GetWarnRecord(BasicBlock* bb, std::set<WarningType>& result);
	void RecordWarn(WarningType);
	std::string GetWarningTypes();
};

#endif
