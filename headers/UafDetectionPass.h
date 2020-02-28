#ifndef _UAF_DP_H
#define _UAF_DP_H

#include "AnalysisStructHeaders.h"
#include "KMDriver.h"
#include <thread>
#include <llvm/Support/ThreadPool.h>
#include <queue>

//using namespace llvm;
class UafDetectionPass final: public IterativeModulePass {
private:
    std::string targetMehtod;
    std::string tmS1;
    std::string tmS2;
    GlobalContext *globalContext;
    // std::queue<std::shared_ptr<AnalysisTask>> todoTasks;
    std::vector<std::shared_ptr<AnalysisTask>> todoTasks;
    size_t threadPoolLimit = 0;

    Type *IntPtrTy = NULL;
    // type for universal pointer, e.g., char * and void *
    Type *Int8Ty = NULL;
    FunctionWrapper* funcWrapper = NULL;

    // Record unsafe allocationss
    bool AnalyzeInst(Instruction*, std::shared_ptr<AnalysisState>);
    bool AnalyzeBasicBlock(BasicBlock* bb, std::shared_ptr<AnalysisState> as, Instruction* startInst, ExecuteRelationship, bool analyzeStartInst);
    bool AnalyzeFromSelectInst(SelectInst* si, std::shared_ptr<AnalysisState> as);
    bool AnalyzeLoadInst(LoadInst* li, std::shared_ptr<AnalysisState>as);
    bool AnalyzeStoreInst(StoreInst* li, std::shared_ptr<AnalysisState>as);
    bool AnalyzeCastInst(CastInst* li, std::shared_ptr<AnalysisState>as);
    bool AnalyzePHINode(PHINode* li, std::shared_ptr<AnalysisState>as);
    bool AnalyzeSelectInst(SelectInst* li, std::shared_ptr<AnalysisState>as);
    bool AnalyzeCallInst(CallInst* li, std::shared_ptr<AnalysisState>as);
    void GenerateFakeRetvalue(CallInst* li, std::shared_ptr<AnalysisState>as);
    bool AnalyzeReturnInst(ReturnInst* li, std::shared_ptr<AnalysisState>as);
    bool AnalyzeCmpInst(CmpInst* ci, std::shared_ptr<AnalysisState> as);
    bool AnalyzeBiOpe(BinaryOperator* bo, std::shared_ptr<AnalysisState> as);
    bool AnalyzeAndInst(BinaryOperator* bo, std::shared_ptr<AnalysisState> as, Value* v1, Value* v2);
    bool AnalyzeOrInst(BinaryOperator* bo, std::shared_ptr<AnalysisState> as, Value* v1, Value* v2);
    bool AnalyzeGEPInst(GetElementPtrInst* gepInst, std::shared_ptr<AnalysisState>as);
    bool IsLLVMDebugFunction(Function* func);
    bool IsLLVMDebugInst(Instruction*);

    bool AnalyzeBranchInst(BranchInst* br, std::shared_ptr<AnalysisState> as);
    bool AnalyzeSwitchInst(SwitchInst* swInst, std::shared_ptr<AnalysisState> as);

    void GenerateArgMemBlock(Function* func, std::shared_ptr<CallRecord> callRecord, std::shared_ptr<AnalysisState> as);
    static void ExitThreadPool(GlobalContext* gc, UafDetectionPass* pass, AnalysisTask* at);

public:
    UafDetectionPass(std::string tm, bool isSingleThread, GlobalContext* gc);
    UafDetectionPass(std::string tm1, std::string tm2, bool isSingleThread, GlobalContext* gc);
    ~UafDetectionPass();

    static void* TaskHandlerThread(void* arg);
    static void* SysMemWatchdog(void* arg);

    static void PrintThreadSum(UafDetectionPass* pass, std::string thdStr);
    static bool CheckConstantEqual(Constant* c1, Constant* c2);

    void CreateAnalysisTask_NoLock(BasicBlock* bb,
    		std::shared_ptr<AnalysisState> as, Instruction* si, ExecuteRelationship er, std::string thread);

    virtual bool doModulePass(llvm::Module *);
    virtual bool doInitialization(llvm::Module *);
    virtual bool doFinalization(llvm::Module *);
    void DetectUAF(Function*);
    void DetectUAF_MultiSteps(Function*, Function*);
    std::shared_ptr<MemoryBlock> AnalyzeGEPOperator(GEPOperator* li, AnalysisState* as, std::map<std::shared_ptr<AnalysisState>, std::shared_ptr<MemoryBlock>>* asSet = NULL);
    std::shared_ptr<MemoryBlock> HandleGepIndex(GEPOperator* gep, uint64_t index, AnalysisState* as, std::shared_ptr<MemoryBlock> dcMB, std::map<std::shared_ptr<AnalysisState>, std::shared_ptr<MemoryBlock>>* newASSet);
	static std::string GetBasicBlockLines(BasicBlock* bb);

};

#endif
