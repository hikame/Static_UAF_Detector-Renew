//===-- UafDetectionPass.cc - Finding unsafe allocations ---------===//
// 
// This pass conservatively identifies UAF vulnerabilities.
//===-----------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/TypeFinder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/CallSite.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringMap.h"
#include <set>
#include <map>
#include <queue>
#include <vector>
#include <list>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>

#include "headers/Config.h"
#include "headers/Common.h"
#include "headers/UafDetectionPass.h"
#include "headers/DataLayoutHelper.h"

#define TARGETNAME "KMDriverTestFunc"
#define MEMTHREADHOLD 20  // todo

using namespace llvm;

// The iterative framework
bool UafDetectionPass::doInitialization(Module *M) {
	const DataLayout* DL = &(M->getDataLayout()); //KM: class DataLayout
  IntPtrTy = DL->getIntPtrType(M->getContext());
  Int8Ty = Type::getInt8Ty(M->getContext());
  globalContext->NumFunctions += M->size();
  globalContext->dlHelper = new DataLayoutHelper(DL, globalContext);
  globalContext->UDP = this;
  globalContext->llvmContext = &M->getContext();
  globalContext->ssm = new UAFSourceSinkManager(globalContext);
  return false;
}

bool UafDetectionPass::doModulePass(Module *M) {
	Function *f1 = NULL;
	Function *f2 = NULL;
	Function *tf = NULL;
	for (Module::iterator f = M->begin(), fe = M->end(); f != fe; ++f) {
		Function *F = &*f;
		if(F->getName().str() == tmS1){
			f1 = F;
			continue;
		}
		else if(F->getName().str() == tmS2){
			f2 = F;
			continue;
		}
		else if(F->getName().str() == targetMehtod){
			tf = F;
		}
	}
	if(tf != NULL)
		DetectUAF(tf);
	else if(f1 != NULL && f2 != NULL)
		DetectUAF_MultiSteps(f1, f2);
  return false;
}


bool UafDetectionPass::doFinalization(Module *M) {
  return false;
}

void UafDetectionPass::DetectUAF_MultiSteps(Function* f1, Function* f2){
	std::shared_ptr<AnalysisState> as = std::shared_ptr<AnalysisState>(new AnalysisState(&f1->getContext(), globalContext));
	std::string thdStr = GetThreadID();
	{ 	//Step 1
		GenerateArgMemBlock(f1, NULL, as);
		Function::iterator bbit = f1->getBasicBlockList().begin();

		CreateAnalysisTask_NoLock(&*bbit, as, NULL, Start, thdStr);
	}

	as->PrepareForStep2(); //TODO this will already been freed!

	{ // Step 2
		GenerateArgMemBlock(f2, NULL, as);
		Function::iterator bbit = f2->getBasicBlockList().begin();
		CreateAnalysisTask_NoLock(&*bbit, as, NULL, Start, thdStr);
	}
}

void UafDetectionPass::DetectUAF(Function* targetFunc){
  	// start watch dog thread of system memory usage
	pthread_t tid = 0;
	if(pthread_create(&tid, NULL, SysMemWatchdog, (void*)globalContext) != 0)
		perror("Cannot create new thread.");

	std::shared_ptr<AnalysisState> as = 
		std::shared_ptr<AnalysisState>(new AnalysisState(&targetFunc->getContext(), globalContext));
	std::shared_ptr<CallRecord> cr = as->AddNewCallRecord(NULL, targetFunc);
	GenerateArgMemBlock(targetFunc, cr, as);
	Function::iterator bbit = targetFunc->getBasicBlockList().begin();
	CreateAnalysisTask_NoLock(&*bbit, as, NULL, Start, GetThreadID());

	struct timeval tv;
	gettimeofday(&tv, NULL);
	long last_sec = tv.tv_sec;
	std::set<std::shared_ptr<AnalysisTask>> runningTasks;
	size_t finished = 0;
	size_t errCount = 0;
	while(true){
		// wait for some thread to exit
		while(runningTasks.size() >= threadPoolLimit // current running thread exceed the limit
				|| (todoTasks.size() == 0 && runningTasks.size() != 0)){
			// if(globalContext->shouldQuit) // if we are notified to break;
			// 	break;
			
			// wait for some thread to finish, and take them out
			auto rt = runningTasks.begin(); 
			bool foundFinished = false;
			while(rt != runningTasks.end()){
				if((*rt)->finished){
					finished++;
					if((*rt)->analysisState->hasError)
						errCount++;
					rt = runningTasks.erase(rt);
					foundFinished = true;
					
				}
				else
					rt++;
			}
			if(foundFinished)
				continue;
			usleep(100); 
			
			// check do we need out logs
			gettimeofday(&tv, NULL);
			if(tv.tv_sec - last_sec >= 10){
				globalContext->opLock.lock();
				OP 	<< "[Tread-" << GetThreadID() << "] [INF] [" 
					<< GetCurrentTime() << "] Todo vs Finished vs Errors: " 
					<< todoTasks.size() << " vs " << finished << " vs " << errCount << ".\n";
				globalContext->opLock.unlock(); 
				last_sec = tv.tv_sec;
			}
		}

		// check if all tasks have finished or the process should quit
		if(runningTasks.size() == 0 && todoTasks.size() == 0){
			globalContext->shouldQuit = true; // notify watchdog to exit
			OP 	<< "[Tread-" << GetThreadID() << "] [INF] [" 
				<< GetCurrentTime() << "] All tasks have finished.\n";
		}
		if(globalContext->shouldQuit){
			if(runningTasks.size() != 0){
				auto rt = runningTasks.begin(); 
				while(rt != runningTasks.end()){
					if((*rt)->finished){
						finished++;
						rt = runningTasks.erase(rt);
					}
					else
						rt++;
				}
				usleep(100); 
				continue;
			}
			else
				break;
		}

		size_t space = threadPoolLimit - runningTasks.size();
		std::shared_ptr<AnalysisTask> tsk[space];
		// takeout a new task
		globalContext->tpLock.lock();
		for(size_t i = 0; i < space; i++){
			tsk[i] = todoTasks.front();
			todoTasks.pop();
			if(todoTasks.size() == 0)
				space = i + 1;
		}
		globalContext->tpLock.unlock();
		
		for(size_t i = 0; i < space; i++){
			if(pthread_create(&tid, NULL, TaskHandlerThread, tsk[i].get()) != 0)
				perror("Cannot create new thread.");
			runningTasks.insert(tsk[i]);
		}
	}
	todoTasks = std::queue<std::shared_ptr<AnalysisTask>>(); // clear todo list
	OP << "[Tread-" << GetThreadID() << "] " << "[INF] Analysis Finished @ " << GetCurrentTime() << "\n";
}

void* UafDetectionPass::SysMemWatchdog(void* arg){
	GlobalContext* gc = (GlobalContext*) arg;
	gc->opLock.lock();
	OP 	<< "[Tread-" << GetThreadID() << "] " 
		<< "[INF] System memory watchdog started @ "
		<< GetCurrentTime() << ".\n";
	gc->opLock.unlock();

	size_t unaCount = 0; 
	size_t waittimes = 0;
	while(!gc->shouldQuit){
		FILE *fd =fopen("/proc/meminfo", "r");
		if(nullptr == fd){
			OP << "[ERR] Cannot open /proc/meminfo.\n";
			exit(-1);
		}
		usleep(1000 * 100);
		fseek(fd, 0, SEEK_SET);
		char line_buff[128];
		char name[64];
		long long tmp = -1, memtotal = -1, memfree = -1, swaptotal = -1, swapfree = -1;
		while(true){
			char* ret = fgets(line_buff,sizeof(line_buff),fd);
			if(ret == NULL){
				OP << "[ERR] Cannot read MemTotal & MemAvailable in /proc/meminfo.\n";
				exit(-1);
			}
			sscanf(line_buff,"%s %lld", name, &tmp);
			if(strcmp(name, "MemTotal:") == 0)
				memtotal = tmp;
			else if(strcmp(name, "MemFree:") == 0)
				memfree = tmp;
			else if(strcmp(name, "SwapTotal:") == 0)
				swaptotal = tmp;
			else if(strcmp(name, "SwapFree:") == 0)
				swapfree = tmp;
			if(memtotal != -1 && memfree != -1 && swaptotal != -1 && swapfree != -1){
				double perc = (memfree + swapfree) * 100 / (memtotal + swaptotal);
				gc->memAvaliable = (perc > MEMTHREADHOLD);
				break;
			}
		}
		fclose(fd);
		if(gc->memAvaliable)
			unaCount = 0;
		else{
			unaCount++;
			if(unaCount >= 100){ // do log every 10s (100*100 us)
				gc->opLock.lock();
				OP 	<< "[Tread-" << GetThreadID() << "] " 
				 	<< "[" << GetCurrentTime() << "] [INF] NO ENOUGH MEMEORY...\n";
				gc->opLock.unlock();
				waittimes++;
				if(waittimes >= 6) // 60 seconds to wait for more memory
					break;
				unaCount = 0;
			}
		}
	}
	gc->shouldQuit = true;
	return NULL;
}

void* UafDetectionPass::TaskHandlerThread(void* arg){
	AnalysisTask* at = (AnalysisTask*) arg;
	std::shared_ptr<AnalysisState> as = at->analysisState;
	UafDetectionPass* pass = at->uafPass;
	at->started = true;
	pthread_detach(pthread_self());

	if(as->globalContext->shouldQuit){
		at->finished = true;
		return NULL;
	}
	BasicBlock* bb = at->basicBlock;
	Instruction* startInst = at->startInst;
	std::string parentThread = at->parentThread;
	ExecuteRelationship execRS = at->execRS;

	bool analyzeStartInst = true;
	if(	execRS == Select_True ||
			execRS == Select_False ||
			execRS == MallocResult_Success ||
			execRS == MallocResult_Failed ||
			execRS == Fake_Return_Null ||
			execRS == Fake_Return_Nonnull ||
			execRS == CMP_True ||
			execRS == CMP_False ||
			execRS == Symbolic_Index)
		analyzeStartInst = false;

	std::string thdStr = GetThreadID();
	if(as->globalContext->printDB){
		pass->globalContext->opLock.lock();
		OP << "[Tread-" << parentThread << "] "
				<< "[DBG] Create thread-" << thdStr
				<< " for next basic block: " << UafDetectionPass::GetBasicBlockLines(bb)
				<< ", for reason: " << GetERString(execRS) << "\n";
		OP << "[Tread-" << thdStr << "] " << "[DBG] THREAD STARTED @ " << GetCurrentTime() << "\n";
		pass->globalContext->opLock.unlock();
	}

	bool terminate = pass->AnalyzeBasicBlock(bb, as, startInst, execRS, analyzeStartInst);
	while(!terminate){
		CallInst* callerInst = as->TakeOutLastCall();
		if(callerInst == NULL)
			break;
		BasicBlock* callerBB = callerInst->getParent();
		terminate = pass->AnalyzeBasicBlock(callerBB, as, callerInst, Return, false);
	}
	PrintThreadSum(pass, thdStr);
	at->finished = true;
	return NULL;
}

bool UafDetectionPass::AnalyzeBasicBlock(BasicBlock* bb, std::shared_ptr<AnalysisState> as,
		Instruction* startInst, ExecuteRelationship er, bool analyzeStartInst){
	if((er == Branch_False ||
			er == Branch_True ||
			er == Branch_Undetermined ||
			er == Branch_UnConditional ||
			er == Switch_Sure ||
			er == Switch_NotSure)
			&& as->ShouldNotContinue(bb))
		return true;

	BasicBlock::iterator instIT;
	if(startInst == NULL){
		instIT = bb->getInstList().begin();
	}
	else //this is for the selectinst. it will not record the basic block into the excetion relationship.
		instIT = BasicBlock::iterator(startInst);
	as->AddAnalyzedBasicBlock(bb, er);

	if(!analyzeStartInst)
		instIT++;
	as->PrintExectutionPath();
	while(instIT != bb->getInstList().end()){
		Instruction* inst = &*instIT;
		bool terminate = AnalyzeInst(inst, as);
		if(terminate)
			return true;
		instIT++;
	}

	Instruction *TInst = bb->getTerminator();
	if(BranchInst* brInst = dyn_cast<BranchInst>(TInst)){
		bool terminate = AnalyzeBranchInst(brInst, as);
		if(terminate)
			return true;
	}
	else if(SwitchInst* swInst = dyn_cast<SwitchInst>(TInst)){
		bool terminate = AnalyzeSwitchInst(swInst, as);
		if(terminate)
			return true;
	}

	return false;
}

bool UafDetectionPass::AnalyzeBranchInst(BranchInst* brInst, std::shared_ptr<AnalysisState> as){
	BasicBlock *succBB = NULL;
	ExecuteRelationship er = TODET;
	Value* trueValue = ConstantInt::getTrue(brInst->getContext());
	Value* falseValue = ConstantInt::getFalse(brInst->getContext());
	Value* cVariable = NULL;
	if(brInst->isConditional()){
		cVariable = brInst->getCondition();
		if(auto valueRecord = as->QueryVariableRecord(cVariable)){
			if(auto cvw = std::dynamic_pointer_cast<ConstantValueWrapper>(valueRecord)){
				Value* value = cvw->containedValue;
				if(value == trueValue){
					succBB = brInst->getSuccessor(0);
					er = Branch_True;
				}
				else if(value == falseValue){
					succBB = brInst->getSuccessor(1);
					er = Branch_False;
				}
			}
		}
	}
	else{
		succBB = brInst->getSuccessor(0);
		er = Branch_UnConditional;
	}
	if(succBB != NULL){
		bool ret = AnalyzeBasicBlock(succBB, as, NULL, er, true);
		return ret;
	}
	//The following code should never be executed!
	BasicBlock* trueSB = brInst->getSuccessor(0);
	BasicBlock* falseSB = brInst->getSuccessor(1);
	std::shared_ptr<AnalysisState> trueAS = as->MakeCopy();
	if(trueAS == NULL)
		return true;
	std::shared_ptr<AnalysisState> falseAS = as->MakeCopy();
	if(falseAS == NULL)
		return true;

	trueAS->AddLocalVariableRecord(cVariable, std::shared_ptr<ConstantValueWrapper>(new ConstantValueWrapper(trueValue)));
	falseAS->AddLocalVariableRecord(cVariable, std::shared_ptr<ConstantValueWrapper>(new ConstantValueWrapper(falseValue)));
	globalContext->tpLock.lock();
	CreateAnalysisTask_NoLock(trueSB, trueAS, NULL, Branch_Undetermined, GetThreadID());
	CreateAnalysisTask_NoLock(falseSB, falseAS, NULL, Branch_Undetermined, GetThreadID());
	globalContext->tpLock.unlock();
	return true;
}

bool UafDetectionPass::AnalyzeSwitchInst(SwitchInst* swInst, std::shared_ptr<AnalysisState> as){
	Value* cVariable = swInst->getCondition();
	ConstantInt* ci = dyn_cast<ConstantInt>(cVariable);
	if(ci == NULL)
		if(auto cRecord = as->QueryVariableRecord(cVariable))
			if(auto cwr = std::dynamic_pointer_cast<ConstantValueWrapper>(cRecord))
				ci = dyn_cast<ConstantInt>(cwr->containedValue);
			
	if(ci == NULL){
		std::string thdStr = GetThreadID();
		std::map<BasicBlock*, std::shared_ptr<AnalysisState>> todoMap;
		BasicBlock *succBB = swInst->getDefaultDest();
		std::shared_ptr<AnalysisState> branchAS = as->MakeCopy();
		if(branchAS == NULL)
			return true;
		todoMap[succBB] = branchAS;
		SwitchInst::CaseIt i = swInst->case_begin();
		SwitchInst::CaseIt e = swInst->case_end();
		for (; i != e; ++i) {
			BasicBlock* bb = i->getCaseSuccessor();
			ConstantInt* caseV = i->getCaseValue();
			std::shared_ptr<AnalysisState> branchAS = as->MakeCopy();
			if(branchAS == NULL)
				return true;
			branchAS->AddLocalVariableRecord(cVariable, std::shared_ptr<ConstantValueWrapper>(new ConstantValueWrapper(caseV)));
			todoMap[bb] = branchAS;
   		}

		globalContext->tpLock.lock();
		for(auto todo : todoMap)
			CreateAnalysisTask_NoLock(todo.first, todo.second, NULL, Switch_NotSure, thdStr);
		globalContext->tpLock.unlock();

		return true;
	}
	else{
		BasicBlock *succBB = swInst->findCaseValue(ci)->getCaseSuccessor();
		bool ret = AnalyzeBasicBlock(succBB, as, NULL, Switch_Sure, true);
		return ret;
	}
}


bool UafDetectionPass::IsLLVMDebugInst(Instruction* targetInst){
	CallInst* ci = dyn_cast<CallInst>(targetInst);
	if(ci == NULL)
		return false;

	Function *func;
	if (!(func = ci->getCalledFunction())) {
		Value* v = ci->getCalledValue()->stripPointerCasts();
		if(!(func = dyn_cast<Function>(&*v))){
			return false;
		}
	}

	if(IsLLVMDebugFunction(func))
		return true;
	return false;
}

bool UafDetectionPass::AnalyzeInst(Instruction* targetInst, std::shared_ptr<AnalysisState> as){
	if(globalContext->shouldQuit)
		return true;
	if(IsLLVMDebugInst(targetInst))
		return false;
	as->instCount++;

	std::string thdStr = GetThreadID();

	if(as->globalContext->printTC){
		// std::string bbName = targetInst->getParent()->getName().str();
		std::string funcName = targetInst->getParent()->getParent()->getName().str();
		std::string instStr = as->globalContext->GetInstStr(targetInst);
		globalContext->opLock.lock();
		OP << "[Tread-" << thdStr << "] [TRC] Analyzing Inst: "
				<< targetInst->getParent() << " (" << funcName << "): " << instStr << "\n";
		globalContext->opLock.unlock();
	}

	// check if this is a sunk
	if(globalContext->ssm->IsSink(targetInst, as)){
		if(!globalContext->continueAAS)
			return true;
	}

	// add some tag, such as freed.
	int shouldContinue = globalContext->ssm->AnalyzeTag(targetInst, as);
	if(shouldContinue == 0)
		return false; //should not continue
	if(shouldContinue == -1)
		return true; //terminate

	bool terminate = false;
	if(AllocaInst* ai = dyn_cast<AllocaInst>(targetInst))
		as->GenerateStackMB(ai);
	else if (LoadInst *li = dyn_cast<LoadInst>(targetInst))
		terminate = AnalyzeLoadInst(li, as);
	else if(StoreInst *si = dyn_cast<StoreInst>(targetInst))
		terminate = AnalyzeStoreInst(si, as);
	else if(GetElementPtrInst *gi = dyn_cast<GetElementPtrInst>(targetInst))
		terminate = AnalyzeGEPInst(gi, as);
	else if(CastInst *ci = dyn_cast<CastInst>(targetInst))
		terminate = AnalyzeCastInst(ci, as);
	else if(PHINode *phi = dyn_cast<PHINode>(targetInst))
		terminate = AnalyzePHINode(phi, as);
	else if (SelectInst *si = dyn_cast<SelectInst>(targetInst))
		terminate = AnalyzeSelectInst(si, as);
	else if (CallInst *ci = dyn_cast<CallInst>(targetInst))
		terminate = AnalyzeCallInst(ci, as);
	else if (ReturnInst *ri = dyn_cast<ReturnInst>(targetInst))
		terminate = AnalyzeReturnInst(ri, as);
	else if (CmpInst* ci = dyn_cast<CmpInst>(targetInst))
		terminate = AnalyzeCmpInst(ci, as);
	else if (BinaryOperator * bo = dyn_cast<BinaryOperator>(targetInst))
		terminate = AnalyzeBiOpe(bo, as);
	else if(isa<ExtractValueInst>(targetInst)){
		std::shared_ptr<SymbolicValue> sv(new SymbolicValue());
		as->AddLocalVariableRecord(targetInst, sv);
		terminate = false;
	}
	// TODO I just want to see what are they mean
	else if(isa<AtomicCmpXchgInst>(targetInst) ||
				isa<InsertElementInst>(targetInst) ||
				isa<ExtractElementInst>(targetInst) ||
				isa<InsertValueInst>(targetInst) ||
				isa<LandingPadInst>(targetInst) ||
				isa<ShuffleVectorInst>(targetInst) ||
				isa<InvokeInst>(targetInst) ||
				isa<VAArgInst>(targetInst)){
		if(as->globalContext->printER){
			globalContext->opLock.lock();
			OP << "[Tread-" << thdStr << "] " <<
					" [ERR] We have meet an interesting instruction, let's have a look!\n";
			OP << "[Tread-" << thdStr << "] " <<
					" [ERR] Inst is " << globalContext->GetInstStr(targetInst) << "\n";
			OP << "[Tread-" << thdStr << "] " <<
					" [ERR] From Func " << targetInst->getParent()->getParent()->getName() << "\n";
			globalContext->opLock.unlock();
		}
		exit(0);
	}
	//   Ignore following instructions.
	else if(isa<BinaryOperator>(targetInst) ||
				isa<CmpInst>(targetInst) ||
				isa<AtomicRMWInst>(targetInst) ||
				isa<FenceInst>(targetInst) ||
				isa<FuncletPadInst>(targetInst) ||
				isa<CmpInst>(targetInst) ||
				isa<TerminatorInst>(targetInst))
			return false;
	else{
		if(as->globalContext->printER){
			as->globalContext->opLock.lock();
			OP<<"[ERR] !!! Unrecognized Inst: " << globalContext->GetInstStr(targetInst) << "\n";
			as->globalContext->opLock.unlock();
		}
		exit(0);
	}

	return terminate;
}

bool UafDetectionPass::AnalyzeLoadInst(LoadInst* li, std::shared_ptr<AnalysisState>as){
	Value* value = li;
	Value* pOpe = li->getPointerOperand();
	
	std::shared_ptr<MemoryBlock> pointerMB = NULL;
	if(auto cvr = as->GetVariableRecord(pOpe)){
		pointerMB = std::dynamic_pointer_cast<MemoryBlock>(cvr); //direct container
		if(pointerMB == NULL){
			if(auto sv = std::dynamic_pointer_cast<SymbolicValue>(cvr)){
				pointerMB = std::shared_ptr<MemoryBlock>(new MemoryBlock(pOpe, Heap, as->globalContext, true));
				// as->AddVariableRecord(pOpe, pointerMB); //todo delete, following merge will do this
				as->MergeFakeValueRecord(cvr, pointerMB);
			}
			else{
				if(globalContext->printER){
					std::lock_guard<std::mutex> lg(globalContext->opLock);
					OP << "[Tread-" << GetThreadID() << "] [ERR] Load from a pointer, which does not points to a memory block: "
							<< globalContext->GetInstStr(li) << "\n";
				}
				as->hasError = true;
				return true;
			}
		}
	}
	else{  //cvr is NULL 
		// pointerMB = std::shared_ptr<MemoryBlock>(new MemoryBlock(pOpe, Heap, as->globalContext, true));
		// as->AddVariableRecord(pOpe, pointerMB);
		assert(0);  // GetVariableRecord will always return some
	}

	Type* vType = li->getType();
	uint64_t size = as->globalContext->dlHelper->GetTypeStoreSize(vType);
	while(pointerMB != NULL && size < pointerMB->GetSize()){
		std::shared_ptr<FieldRelationship> fr = pointerMB->getField(as.get(), (int64_t)0);
		if(fr == NULL){
			if(globalContext->printWN){
				std::lock_guard<std::mutex> lg(globalContext->opLock);
				OP 	<< "[Tread-" << GetThreadID()
						<< "] [WRN] The source memory block of load is larger than the target type: "
						<< globalContext->GetInstStr(li) << "\n";
			}
			as->RecordWarn(Load_from_Larger_MemoryBlock);
			break;
		}
		pointerMB = fr->GetFieldBlock();
	}

	auto valueInMB = as->GetMBContainedValue(pointerMB);
	as->AddLocalVariableRecord(value, valueInMB);
	return false;
}

bool UafDetectionPass::AnalyzeStoreInst(StoreInst* si, std::shared_ptr<AnalysisState>as){
	Value* vOpe = si->getValueOperand();
	Value* pOpe = si->getPointerOperand();

	std::shared_ptr<MemoryBlock> pointerMB = NULL;
	if(auto cvr = as->GetVariableRecord(pOpe)){
		pointerMB = std::dynamic_pointer_cast<MemoryBlock>(cvr); //direct container
		if(pointerMB == NULL){
			if(auto sv = std::dynamic_pointer_cast<SymbolicValue>(cvr)){
				pointerMB = std::shared_ptr<MemoryBlock>(new MemoryBlock(pOpe, Heap, as->globalContext, true));
				// as->AddVariableRecord(pOpe, pointerMB); todo delete, this will be down by Merge...
				as->MergeFakeValueRecord(cvr, pointerMB);
			}
			else{
				if(globalContext->printER){
					std::lock_guard<std::mutex> lg(globalContext->opLock);
					OP << "[Tread-" << GetThreadID() << "] [ERR] Store to a pointer, which does not points to a memory block: "
							<< globalContext->GetInstStr(si) << "\n";
				}
				as->hasError = true;
				return true;
			}
		}
	}
	else{  //cvr is NULL 
		// pointerMB = std::shared_ptr<MemoryBlock>(new MemoryBlock(pOpe, Heap, as->globalContext, true));
		// as->AddVariableRecord(pOpe, pointerMB);
		assert(0); // GetVariableRecord will always return some
	}

	Type* vType = vOpe->getType();
	uint64_t size = as->globalContext->dlHelper->GetTypeStoreSize(vType);
	while(pointerMB != NULL && size < pointerMB->GetSize()){
		std::shared_ptr<FieldRelationship> fr = pointerMB->getField(as.get(), (int64_t)0);
		if(fr == NULL){
			if(globalContext->printWN){
				std::lock_guard<std::mutex> lg(globalContext->opLock);
				OP 	<< "[Tread-" << GetThreadID()
						<< "] [WRN] The dest memory block of store is larger than the target type: "
						<< globalContext->GetInstStr(si) << "\n";
			}
			as->RecordWarn(Store_to_Larger_MemoryBlock);
			break;
		}
		pointerMB = fr->GetFieldBlock();
	}
	
	auto newVR = as->GetVariableRecord(vOpe);
	as->RecordMBContainedValue(pointerMB, newVR);
	return false;
}

// return NULL if we cannot handle this get element operation
std::shared_ptr<MemoryBlock> UafDetectionPass::HandleGepIndex(GEPOperator* gep, 
		uint64_t index, AnalysisState* as, std::shared_ptr<MemoryBlock> dcMB, 
		std::map<std::shared_ptr<AnalysisState>, std::shared_ptr<MemoryBlock>>* newASSet){
	if(dcMB == NULL)
		return NULL;

	int idxNum = gep->getNumIndices();
	if(index > idxNum)  // bypass the last indict
		return dcMB;

	// Type* resultEleType = gep->getType();

	Value* indexOpe = gep->getOperand(index);
	std::shared_ptr<StoredElement> indexVR = as->GetVariableRecord(indexOpe);

	// std::lock_guard<std::mutex> lg(dcMB->frLock);
	// auto nextFR = dcMB->FindFieldRelationship_NoLock(indexVR, 0); //indexVR can be a symbolic value or constant data
	// if(nextFR == NULL){
	std::shared_ptr<FieldRelationship> nextFR = NULL;
	if(auto indexSV = std::dynamic_pointer_cast<SymbolicValue>(indexVR)){ //the index is a symbolic value
		if(index != idxNum){
			if(globalContext->printWN){
				std::lock_guard<std::mutex> lg(globalContext->opLock);
				OP 	<< "[Tread-" << GetThreadID()
						<< "] [ERR] Get element with a dynamic index and this index is not the last one. Not support currently..."
						<< globalContext->GetInstStr(gep) << "\n";
			}
			as->RecordWarn(GEP_with_Symbolic_Index);
			return NULL;
		}
				
		nextFR = dcMB->getField(as, indexSV);
		if(nextFR == NULL)
			return NULL;
		auto sfmb = nextFR->GetFieldBlock();
		if(newASSet != NULL){  // the caller wants all the possible contained values for this field
			if(isa<ArrayType>(dcMB->valueType)){
				Type* resultEleType = gep->getResultElementType();  // also is sfmb->valueType
				if(!isa<ArrayType>(resultEleType) && !isa<StructType>(resultEleType)){ // if is Array or Struct, we don't support the record all the possible field relationships
					// current analysisstate may contain some record
					for(auto fr : dcMB->GetFields_NoLock()){ //get all the value of the fields
						if(std::dynamic_pointer_cast<SymbolicValue>(fr->indexVR))
							continue;
						auto record = as->GetMBContainedValue(fr->GetFieldBlock(), false);  // query without create new ones
						if(record != NULL){
							std::shared_ptr<AnalysisState> newAS = as->MakeCopy();
							newAS->RecordMBContainedValue(sfmb, record);
							newAS->CopyMemoryBlockTags(sfmb, fr->GetFieldBlock().get());
							newASSet->insert(std::pair<std::shared_ptr<AnalysisState>, std::shared_ptr<MemoryBlock>>(newAS, sfmb));
						}
					}
				}
			}
			else{
				if(globalContext->printWN){
					std::lock_guard<std::mutex> lg(globalContext->opLock);
					OP 	<< "[Tread-" << GetThreadID()
							<< "] [WRN] Get element with a dynamic index from a non-ArrayType. Very Strange:"
							<< globalContext->GetInstStr(gep) << "\n";
				}
				as->RecordWarn(GEP_with_Symbolic_Index_from_NonArrayType);
				return NULL;
			}
		}
	}
	else{ // indexVR is a ConstantValueWrapper
		auto cvr = std::dynamic_pointer_cast<ConstantValueWrapper>(indexVR);  // should always success
		ConstantInt* indexCI = dyn_cast<ConstantInt>(cvr->containedValue);
		assert(indexCI != NULL);
		
		if(indexCI->getSExtValue() < 0)
			return NULL;
		else{ //this may not be the right answer!
			unsigned eleIndex = indexCI->getZExtValue();
			nextFR = dcMB->getField(as, eleIndex);
			if(nextFR == NULL)
				return NULL;
		}
	}

	dcMB = nextFR->GetFieldBlock();
	return HandleGepIndex(gep, index + 1, as, dcMB, newASSet);  //nested call
}

bool UafDetectionPass::AnalyzeGEPInst(GetElementPtrInst* gepInst, std::shared_ptr<AnalysisState> as){
	std::map<std::shared_ptr<AnalysisState>, std::shared_ptr<MemoryBlock>> newASSet;
	std::shared_ptr<MemoryBlock> mb = AnalyzeGEPOperator(dyn_cast<GEPOperator>(gepInst), as.get(), &newASSet);
	if(globalContext->shouldQuit) // the analysis may should quit
		return true;
	if(mb == NULL){
		if(globalContext->printER){
			std::lock_guard<std::mutex> lg(globalContext->opLock);
			OP 	<< "[Tread-" << GetThreadID()
					<< "] [ERR] Cannot get field memoryblock of target gep instruction: "
					<< globalContext->GetInstStr(gepInst) << ".\n";
		}
		as->hasError = true;
		return true;
	}
	if(newASSet.size() == 0){
		as->AddLocalVariableRecord(gepInst, mb);
		return false;
	}
	// the returned mb is a field with symbolic index, it may contains different values
	globalContext->tpLock.lock();
	for(std::pair<std::shared_ptr<AnalysisState>, std::shared_ptr<MemoryBlock>> pair : newASSet){
		std::shared_ptr<AnalysisState> newAS = pair.first;
		std::shared_ptr<MemoryBlock> mb = pair.second;
		newAS->AddLocalVariableRecord(gepInst, mb);
		CreateAnalysisTask_NoLock(gepInst->getParent(), newAS, gepInst, Symbolic_Index, GetThreadID());
	}
	globalContext->tpLock.unlock();
	return true;
}

std::shared_ptr<MemoryBlock> UafDetectionPass::AnalyzeGEPOperator(GEPOperator* gep, AnalysisState* as, 
		std::map<std::shared_ptr<AnalysisState>, std::shared_ptr<MemoryBlock>>* newASSet){
	// check if this gep has record in current as, which means it used to be calculated but failed
	auto record = as->QueryVariableRecord(gep);
	if(record != NULL){
		auto ret = std::dynamic_pointer_cast<MemoryBlock>(record);
		return ret;
	}

	Value* cvalue = gep->getPointerOperand();
	auto valueRecord = as->GetVariableRecord(cvalue);
	auto dcMB = std::dynamic_pointer_cast<MemoryBlock>(valueRecord); //direct container
	if(dcMB == NULL){
		if(std::dynamic_pointer_cast<SymbolicValue>(valueRecord)){
			std::shared_ptr<MemoryBlock> createdMB(new MemoryBlock(cvalue, Heap, as->globalContext, true));
			// as->AddVariableRecord(cvalue, createdMB); // todo delete, following merge will do this
			as->MergeFakeValueRecord(valueRecord, createdMB);
			valueRecord = createdMB;
			dcMB = createdMB;
		}
		else if(auto valueWrapper = std::dynamic_pointer_cast<ConstantValueWrapper>(valueRecord)){
			// this is a strange situation, the recored value of th container pointer is a ConstantValueWrapper...
			// update: this is not strange at all, such as valueRecord is NULL

			valueRecord = as->QueryVariableRecord(valueWrapper->containedValue);
			dcMB = std::dynamic_pointer_cast<MemoryBlock>(valueRecord); //unlikely to happen dcMB != NULL
			if(dcMB == NULL){
				if(globalContext->printER){
					std::lock_guard<std::mutex> lg(globalContext->opLock);
					OP 	<< "[Tread-" << GetThreadID()
							<< "] [ERR] Get element from the pointer, which points to a NULL memory block: "
							<< globalContext->GetInstStr(gep) << "\n";
				}
				return NULL;
			}
		}

	}

	Type* resultEleType = gep->getResultElementType(); 
	int64_t resultEleSize = globalContext->dlHelper->GetTypeStoreSize(resultEleType);

	Type* setype = gep->getSourceElementType();

	std::shared_ptr<MemoryBlock> retmb = NULL;
	// handle the first index value, it indicts whether dcMB could be used directly 
	// or we should found it from its container based on the index value
	Value* firstindexOpe = gep->getOperand(1);
	if(auto fiSE = as->QueryVariableRecord(firstindexOpe)){
		if(auto cvw = std::dynamic_pointer_cast<ConstantValueWrapper>(fiSE)){
			if(auto ci = dyn_cast<ConstantInt>(cvw->containedValue)){
				int64_t index = ci->getSExtValue();
				if(index == 0){
					if(setype != dcMB->valueType){
						dcMB->resetType(setype);
					}
					retmb = HandleGepIndex(gep, 2, as, dcMB, newASSet); // use dcMB directly
				}
				else if(index > 0){
					// we need to find dcMB from dcMB's container
					auto cfr = dcMB->GetContainerFR();
					if(cfr == NULL || cfr->containerBlock == NULL){
						if(globalContext->printWN){
							std::lock_guard<std::mutex> lg(globalContext->opLock);
							OP 	<< "[Tread-" << GetThreadID()
									<< "] [WRN] the first index of gep is not 0, but we cannot get the start memoryblock's container: "
									<< globalContext->GetInstStr(gep) << ". Will generate a fake memory block for it.\n";
						}
						as->RecordWarn(GEP_without_Suitable_Container);
					}
					else{
						auto container = cfr->containerBlock;
						if(auto fr = container->getField(as, index)){
							if(setype != dcMB->valueType){
								dcMB->resetType(setype);
							}
							retmb = HandleGepIndex(gep, 2, as, dcMB, newASSet);  // start from the second index
						}
					}
				}
				else{
					if(globalContext->printWN){
						std::lock_guard<std::mutex> lg(globalContext->opLock);
						OP 	<< "[Tread-" << GetThreadID()
								<< "] [WRN] the first index of gep is negative: "
								<< globalContext->GetInstStr(gep) << ". Will generate a fake memory block for it.\n";
					}
					as->RecordWarn(GEP_with_Negative_Index);
				}
			}
			else{
				if(globalContext->printWN){
					std::lock_guard<std::mutex> lg(globalContext->opLock);
					OP 	<< "[Tread-" << GetThreadID()
							<< "] [WRN] the ConstantValueWrapper object of the first index of gep is not a constant int: "
							<< globalContext->GetInstStr(gep) << ". Will generate a fake memory block for it.\n";
				}
				as->RecordWarn(GEP_with_Strange_Index);
			}
		}
		else{
			if(globalContext->printWN){
				std::lock_guard<std::mutex> lg(globalContext->opLock);
				OP 	<< "[Tread-" << GetThreadID()
						<< "] [WRN] the first index of gep is not a ConstantValueWrapper object: "
						<< globalContext->GetInstStr(gep) << ". Will generate a fake memory block for it.\n";
			}
			as->RecordWarn(GEP_with_Strange_Index);
		}
	}
	else{
		if(globalContext->printWN){
			std::lock_guard<std::mutex> lg(globalContext->opLock);
			OP 	<< "[Tread-" << GetThreadID()
				<< "] [WRN] the first index of gep has no value record: "
				<< globalContext->GetInstStr(gep) << ". Will generate a fake memory block for it.\n";
		}
		as->RecordWarn(GEP_with_Strange_Index);
	}

	if(retmb == NULL){ // failed to handle the get element operation
		// we should not call GetVariableRecord, because that function will treat gep operations specially
		if(globalContext->printWN){
			std::lock_guard<std::mutex> lg(globalContext->opLock);
			OP 	<< "[Tread-" << GetThreadID()
					<< "] [WRN] Failed to handle the get element operation: "
					<< globalContext->GetInstStr(gep) << ". Will generate a fake memory block for it.\n";
		}
		as->RecordWarn(GEP_Failed);
		auto fakeRecord = std::shared_ptr<MemoryBlock>(new MemoryBlock(Field, this->globalContext, resultEleSize, resultEleType, true));
	}
	return retmb;
}

bool UafDetectionPass::AnalyzeCastInst(CastInst* ci, std::shared_ptr<AnalysisState>as){
	Value* orgvalue = ci->getOperand(0);
	Value* newvalue = ci;
	auto orgVR = as->GetVariableRecord(orgvalue);

	std::shared_ptr<SymbolicValue> sv = std::dynamic_pointer_cast<SymbolicValue>(orgVR);
	std::shared_ptr<MemoryBlock> mb = std::dynamic_pointer_cast<MemoryBlock>(orgVR);
	if(sv != NULL || mb != NULL){ // change the type!
		Type* nvType = newvalue->getType();
		if(PointerType* pt = dyn_cast<PointerType>(nvType)){
			Type* ct = pt->getElementType();
			uint64_t size = globalContext->dlHelper->GetTypeStoreSize(ct);
			if(mb == NULL && sv != NULL){
				std::shared_ptr<MemoryBlock> mbSP(new MemoryBlock(Heap, this->globalContext, size, ct, true));
				as->MergeFakeValueRecord(mbSP, sv);
				mb = mbSP;
			}
			// if(mb != NULL)
			// 	mb->resetType(ct);   // we need to reset but we cannot handle the memory blocks very well...
		}
	}
	as->AddLocalVariableRecord(newvalue, orgVR);
	return false;
}

bool UafDetectionPass::AnalyzePHINode(PHINode* phi, std::shared_ptr<AnalysisState>as){
	Value* orgvalue = NULL;
	Value* newvalue = phi;

	int count = 0;
	unsigned num = phi->getNumIncomingValues();
	for(int i = 0; i < num; i++){
		BasicBlock* incBlock = phi->getIncomingBlock(i);
		bool found = as->IsLastAnalyzedBB(incBlock);

		if(!found)
			continue;
		count ++;
		orgvalue = phi->getIncomingValue(i);
	}
	auto orgVR = as->GetVariableRecord(orgvalue);
	as->AddLocalVariableRecord(newvalue, orgVR);
	return false;
}

bool UafDetectionPass::AnalyzeSelectInst(SelectInst* si, std::shared_ptr<AnalysisState>as){
	Value* newvalue = si;
	Value* truevalue = si->getTrueValue();
	Value* falsevalue = si->getFalseValue();
	std::string thdStr = GetThreadID();

	// 1. true situation
	std::shared_ptr<AnalysisState> trueas = as->MakeCopy();
	if(trueas == NULL)  // need to quit
		return true;
	std::shared_ptr<AnalysisState> falseas = as->MakeCopy();
	if(falseas == NULL) // need to quit
		return true;

	auto trueVR = trueas->GetVariableRecord(truevalue);
	trueas->AddLocalVariableRecord(newvalue, trueVR);

	// 2. false situation
	auto falseVR = falseas->GetVariableRecord(falsevalue);
	falseas->AddLocalVariableRecord(newvalue, falseVR);
	globalContext->tpLock.lock();
	CreateAnalysisTask_NoLock(si->getParent(), trueas, si, Select_True, thdStr);
	CreateAnalysisTask_NoLock(si->getParent(), falseas, si, Select_False, thdStr);
	globalContext->tpLock.unlock();

	return true;
}

bool UafDetectionPass::AnalyzeCallInst(CallInst* ci, std::shared_ptr<AnalysisState>as){
	std::string thdStr = GetThreadID();

	if (ci->isInlineAsm() && globalContext->printWN) {
		globalContext->opLock.lock();
		OP << "[Tread-" << thdStr << "] " << " [WRN] Reaching an inline asm:"
				<< globalContext->GetInstStr(ci) << "!\n";
		globalContext->opLock.unlock();
		as->RecordWarn(Call_Inline_Asm_Function);
		return false;
	}

	bool foundFunc = true;
	bool needFakeResult = false;
	Function *func;
	if (!(func = ci->getCalledFunction())) {
		Value* v = ci->getCalledValue()->stripPointerCasts();
		if(!(func = dyn_cast<Function>(&*v))){
			auto valueR = as->GetVariableRecord(v);
			if(auto cvw = std::dynamic_pointer_cast<ConstantValueWrapper>(valueR)){
				if(Function* tmp = dyn_cast<Function>(cvw->containedValue))
					func = tmp;
				else{
					foundFunc = false;
					needFakeResult = true;
				}
			}
			else{
				if(globalContext->printWN){
					std::lock_guard<std::mutex> lg(globalContext->opLock);
					OP << "[Tread-" << GetThreadID() << "] [WRN] Cannot determine the called function: "
							<< globalContext->GetInstStr(ci) << "\n";
				}
				as->RecordWarn(Call_without_Known_Target_Function);
				foundFunc = false;
				needFakeResult = true;
			}
		}
	}

	if(foundFunc){
		StringRef funcName = "";
		if(func->hasName())
			funcName = func->getName();
		bool saf = as->ShouldAnalyzeFunction(func);
		if(!saf){
			GenerateFakeRetvalue(ci, as);
			return false;
		}
		bool isMalloc = globalContext->ssm->IsSource(ci);
		if(isMalloc){
			std::shared_ptr<AnalysisState> nullas = as->MakeCopy();
			if(nullas == NULL)  // need to quit
				return true;
			std::shared_ptr<AnalysisState> nnas = as->MakeCopy();
			if(nnas == NULL)   // need to quit
				return true;

			PointerType* pt = dyn_cast<PointerType>(ci->getType());
			ConstantPointerNull* nullPointer = ConstantPointerNull::get(pt);
			nullas->AddLocalVariableRecord(ci, std::shared_ptr<ConstantValueWrapper>(new ConstantValueWrapper(nullPointer)));


			Value* value = ci;
			std::shared_ptr<MemoryBlock> mb(new MemoryBlock(value, MBType::Heap, nnas->globalContext));
			nnas->AddLocalVariableRecord(value, mb);
			std::list<std::shared_ptr<ExecutionRecord>>::iterator it = nnas->executionPath.end();
			it--;
			std::shared_ptr<ExecutionRecord> er = *it;
			er->tag.insert(Malloc);

			globalContext->tpLock.lock();
			CreateAnalysisTask_NoLock(ci->getParent(), nullas, ci, MallocResult_Failed, thdStr);
			CreateAnalysisTask_NoLock(ci->getParent(), nnas, ci, MallocResult_Success, thdStr);
			globalContext->tpLock.unlock();
			return true;
		}
		if(func->getBasicBlockList().empty()){
			FunctionWrapper::HandleResult ret = funcWrapper->HandleFunction(func, ci, as);
			if(ret == FunctionWrapper::HandleResult::NeedAbort){
				as->hasError = true;
				return true;
			}
			else if(ret == FunctionWrapper::HandleResult::HasWrapper)
				return false;
			else if(ret == FunctionWrapper::HandleResult::NoWrapper)
				needFakeResult = true;
		}
	}

	bool excLimit = (as->callPath.size() >= as->globalContext->cpLimit);

	if(needFakeResult || excLimit){
		if(globalContext->printWN){
			std::lock_guard<std::mutex> lg(globalContext->opLock);
			if(needFakeResult)
				OP << "[Tread-" << thdStr << "] " <<
					"[WRN] Function has no basic blocks and no wrapper: "
				<< globalContext->GetInstStr(ci) << "\n";
			else// if(excLimit)
				OP << "[Tread-" << thdStr << "] " <<
					"[WRN] The call path has reach the depth limitation: "
				<< globalContext->GetInstStr(ci) << "\n";
		}
		if(needFakeResult)
			as->RecordWarn(Call_Target_has_no_Body);
		else
			as->RecordWarn(Call_Depth_Exceed_Limit);		

		std::shared_ptr<SymbolicValue> sv(new SymbolicValue());
		as->AddLocalVariableRecord(ci, sv);
		return false;
	}

	Function::BasicBlockListType& bbList = func->getBasicBlockList();
	auto cr = as->AddNewCallRecord(ci, func);
	GenerateArgMemBlock(func, cr, as);
	Function::iterator bbit = bbList.begin();
	bool terminate = AnalyzeBasicBlock(&*bbit, as, NULL, Call, true);
	if(terminate)
		return true;
	as->TakeOutLastCall();
	as->AddAnalyzedBasicBlock(ci->getParent(), Return);
	return false;
}

void UafDetectionPass::GenerateFakeRetvalue(CallInst* ci, std::shared_ptr<AnalysisState>as){
	Value* retV = ci->getReturnedArgOperand();
	if(retV == NULL)
		return;
	std::shared_ptr<SymbolicValue> fv(new SymbolicValue());
	as->AddLocalVariableRecord(ci, fv);  // the callrecord has not been moved to callee
}

bool UafDetectionPass::IsLLVMDebugFunction(Function* func){
	std::string funcname = func->getName().str();
	std::string prefix = "llvm.dbg.";
	auto res = std::mismatch(prefix.begin(), prefix.end(), funcname.begin());

	if(res.first == prefix.end())
		return true;

	prefix = "llvm.lifetime.";
	res = std::mismatch(prefix.begin(), prefix.end(), funcname.begin());

	if(res.first == prefix.end())
		return true;

	return false;
}

bool UafDetectionPass::AnalyzeReturnInst(ReturnInst* ri, 
		std::shared_ptr<AnalysisState>as){
	Value* retValue = ri->getReturnValue();
	std::shared_ptr<CallRecord> lastCR = as->GetLastCall();
	CallInst* ci = lastCR->callInst;
	if(retValue == NULL || ci == NULL)
		return false;
	Instruction* newValue = ci;
	auto retVR = as->QueryVariableRecord(retValue);
	if(retVR == NULL){
		if(globalContext->printWN){
			std::lock_guard<std::mutex> lg(globalContext->opLock);
			OP << "[Tread-" << GetThreadID() << "] " <<
								"[WRN] Returning from function but the returned value has no record:" << globalContext->GetInstStr(retValue) 
								<< " @ " << ri->getParent()->getParent()->getName() << "\n";
		}
		as->RecordWarn(Return_without_Value_Record);
	}
	as->AddLocalVariableRecord(newValue, retVR, 1);
	return false;
}


bool UafDetectionPass::AnalyzeBiOpe(BinaryOperator* bo, std::shared_ptr<AnalysisState> as){
	Instruction::BinaryOps ops = bo->getOpcode();
	Value* v1 = bo->getOperand(0);
	Value* v2 = bo->getOperand(1);
	Type* t1 = v1->getType();
	Type* t2 = v2->getType();
	if(t1 != t2)
		return false;

	IntegerType* it = dyn_cast<IntegerType>(t1);
	if(it == NULL)
		return false;

	if(it->getBitWidth() != 1)
		return false;

	if(auto tmp = std::dynamic_pointer_cast<ConstantValueWrapper>(as->GetVariableRecord(v1)))
		v1 = tmp->containedValue;
	if(auto tmp = std::dynamic_pointer_cast<ConstantValueWrapper>(as->GetVariableRecord(v2)))
		v2 = tmp->containedValue;

	switch(ops){
	case Instruction::And:
		return AnalyzeAndInst(bo, as, v1, v2);
	case Instruction::Or:
		return AnalyzeOrInst(bo, as, v1, v2);
	default:
		break;
	}
	return false;
}

bool UafDetectionPass::AnalyzeAndInst(BinaryOperator* bo,
		std::shared_ptr<AnalysisState> as, Value* v1, Value* v2){
	Value* tv = ConstantInt::getTrue(bo->getContext());
	Value* fv = ConstantInt::getFalse(bo->getContext());
	if(v1 == tv && v2 == tv)
		as->AddLocalVariableRecord(bo, std::shared_ptr<ConstantValueWrapper>(new ConstantValueWrapper(tv)));
	else if(v1 == fv || v2 == fv)
		as->AddLocalVariableRecord(bo, std::shared_ptr<ConstantValueWrapper>(new ConstantValueWrapper(fv)));
	return false; // should not terminate
}

bool UafDetectionPass::AnalyzeOrInst(BinaryOperator* bo,
		std::shared_ptr<AnalysisState> as, Value* v1, Value* v2){
	Value* tv = ConstantInt::getTrue(bo->getContext());
	Value* fv = ConstantInt::getFalse(bo->getContext());
	if(v1 == tv || v2 == tv)
		as->AddLocalVariableRecord(bo, std::shared_ptr<ConstantValueWrapper>(new ConstantValueWrapper(tv)));
	else if(v1 == fv && v2 == fv)
		as->AddLocalVariableRecord(bo, std::shared_ptr<ConstantValueWrapper>(new ConstantValueWrapper(fv)));
	return false; // should not terminate
}

bool UafDetectionPass::AnalyzeCmpInst(CmpInst* ci, std::shared_ptr<AnalysisState> as){
	CmpInst::Predicate prd = ci->getPredicate();
	uint64_t num = ci->getNumOperands();
	assert(num == 2);
	std::string thdStr = GetThreadID();

	std::shared_ptr<StoredElement> opValueRecords[num];
	for(uint64_t i = 0; i < num; i++){
		Value* opValue = ci->getOperand(i);
		opValueRecords[i] = as->GetVariableRecord(opValue);
	}
	ConstantInt* trueResult = ConstantInt::getTrue(ci->getContext());
	ConstantInt* falseResult = ConstantInt::getFalse(ci->getContext());
		Value* result = NULL; //true or false

	if(prd == CmpInst::Predicate::ICMP_EQ){
		if(opValueRecords[0].get() == opValueRecords[1].get())
			result = trueResult;
		else{ // &&opValueRecords[0] != opValueRecords[1]
			std::shared_ptr<ConstantValueWrapper> dcop0 = 
				std::dynamic_pointer_cast<ConstantValueWrapper>(opValueRecords[0]);
			std::shared_ptr<ConstantValueWrapper> dcop1 = 
				std::dynamic_pointer_cast<ConstantValueWrapper>(opValueRecords[1]);
			if(dcop0 && dcop1){ 
				bool equal = CheckConstantEqual(
					dyn_cast<Constant>(dcop0->containedValue), 
					dyn_cast<Constant>(dcop1->containedValue));
				result = equal ? trueResult : falseResult;
			}
			else if(std::dynamic_pointer_cast<MemoryBlock>(opValueRecords[0]) 
					&& std::dynamic_pointer_cast<MemoryBlock>(opValueRecords[1]))  // &&opValueRecords[0] != opValueRecords[1]
				result = falseResult;
			else if(std::dynamic_pointer_cast<SymbolicValue>(opValueRecords[0]) == NULL
					&& std::dynamic_pointer_cast<SymbolicValue>(opValueRecords[1]) == NULL)  // one mb and one constant value && opValueRecords[0] != opValueRecords[1]
				result = falseResult;
			else // at least one fake value!
				result = as->QueryCMPRecord(opValueRecords[0], opValueRecords[1], CmpInst::Predicate::ICMP_EQ);
		}
	}
	else if(prd == CmpInst::Predicate::ICMP_NE){
		if(opValueRecords[0].get() == opValueRecords[1].get())
			result = falseResult;
		else{ // &&opValueRecords[0] != opValueRecords[1]
			std::shared_ptr<ConstantValueWrapper> dcop0 = 
				std::dynamic_pointer_cast<ConstantValueWrapper>(opValueRecords[0]);
			std::shared_ptr<ConstantValueWrapper> dcop1 = 
				std::dynamic_pointer_cast<ConstantValueWrapper>(opValueRecords[1]);
			if(dcop0 && dcop1){ 
				bool equal = CheckConstantEqual(
					dyn_cast<Constant>(dcop0->containedValue), 
					dyn_cast<Constant>(dcop1->containedValue)
				);
				result = equal ? falseResult : trueResult;
			}
			else if(std::dynamic_pointer_cast<MemoryBlock>(opValueRecords[0]) 
					&& std::dynamic_pointer_cast<MemoryBlock>(opValueRecords[1])) // &&opValueRecords[0] != opValueRecords[1]
				result = trueResult;  // points to different mb, therefore return true
			else if(std::dynamic_pointer_cast<SymbolicValue>(opValueRecords[0]) == NULL
					&& std::dynamic_pointer_cast<SymbolicValue>(opValueRecords[1]) == NULL)  // &&opValueRecords[0] != opValueRecords[1]
				result = trueResult;
			else // at least one fake value!
				result = as->QueryCMPRecord(opValueRecords[0], opValueRecords[1], CmpInst::Predicate::ICMP_NE);
		}
	}
	else if(globalContext->bbAnaLimit != 1){  // if this is true, we do not do cmp predict for other kind of cmp
		std::shared_ptr<ConstantValueWrapper> dcop0 = std::dynamic_pointer_cast<ConstantValueWrapper>(opValueRecords[0]);
		std::shared_ptr<ConstantValueWrapper> dcop1 = std::dynamic_pointer_cast<ConstantValueWrapper>(opValueRecords[1]);
		ConstantInt* ci0 = NULL;
		ConstantInt* ci1 = NULL;
		if(dcop0 != NULL)
			ci0 = dyn_cast<ConstantInt>(dcop0->containedValue);
		if(dcop1 != NULL)
			ci1 = dyn_cast<ConstantInt>(dcop1->containedValue);

		if(dcop0 && dcop1){ 
			bool equal = CheckConstantEqual(
				dyn_cast<Constant>(dcop0->containedValue), 
				dyn_cast<Constant>(dcop1->containedValue));
			result = equal ? trueResult : falseResult;
		}

		if(ci0 && ci1){
			// this may not be the right answer
			for(int i = 0; i < 2; i++){
				Value* val = ci->getOperand(i);
				if(!isa<ConstantData>(val)){
					std::shared_ptr<SymbolicValue> sv(new SymbolicValue());
					as->AddLocalVariableRecord(val, sv);
					if(globalContext->printDB){
						std::lock_guard<std::mutex> lg(globalContext->opLock);
						OP << "[Tread-" << GetThreadID() << "] " <<
											"[DBG] Generate fake value for variable: " << globalContext->GetInstStr(val) << "\n";
					}
				}
			}

			if(	prd == CmpInst::Predicate::ICMP_SGE ||
					prd == CmpInst::Predicate::ICMP_SGT ||
					prd == CmpInst::Predicate::ICMP_SLE ||
					prd == CmpInst::Predicate::ICMP_SLT){
				int64_t v0 = ci0->getSExtValue();
				int64_t v1 = ci1->getSExtValue();
				if(ci0->getBitWidth() < 64){
					IntegerType* it = IntegerType::get(*as->llvmContext, 64);
					ci0 = ConstantInt::get(it, v0, true);
					v0 = ci0->getSExtValue();
				}
				if(ci1->getBitWidth() < 64){
					IntegerType* it = IntegerType::get(*as->llvmContext, 64);
					ci1 = ConstantInt::get(it, v1, true);
					v1 = ci1->getSExtValue();
				}
				bool bresult = false;
				if(prd == CmpInst::Predicate::ICMP_SGE)
					bresult = (v0 >= v1);
				else if(prd == CmpInst::Predicate::ICMP_SGT)
					bresult = (v0 > v1);
				else if(prd == CmpInst::Predicate::ICMP_SLE)
					bresult = (v0 <= v1);
				else if(prd == CmpInst::Predicate::ICMP_SLT)
					bresult = (v0 < v1);
				result = bresult ? trueResult : falseResult;

			}
			else if(	prd == CmpInst::Predicate::ICMP_UGE ||
					prd == CmpInst::Predicate::ICMP_UGT ||
					prd == CmpInst::Predicate::ICMP_ULE ||
					prd == CmpInst::Predicate::ICMP_ULT){
				uint64_t v0 = ci0->getZExtValue();
				uint64_t v1 = ci1->getZExtValue();

				if(ci0->getBitWidth() < 64 && ci0->getSExtValue() < 0){
					v0 = 0xffffffff00000000 + v0;
				}
				if(ci1->getBitWidth() < 64 && ci1->getSExtValue() < 0){
					v1 = 0xffffffff00000000 + v1;
				}
				bool bresult = false;
				if(prd == CmpInst::Predicate::ICMP_UGE)
					bresult = (v0 >= v1);
				else if(prd == CmpInst::Predicate::ICMP_UGT)
					bresult = (v0 > v1);
				else if(prd == CmpInst::Predicate::ICMP_ULE)
					bresult = (v0 <= v1);
				else if(prd == CmpInst::Predicate::ICMP_ULT)
					bresult = (v0 < v1);
				result = bresult ? trueResult : falseResult;
			} 
		} //ci0 && ci1
	}
		
	if(result != NULL){
		as->AddLocalVariableRecord(ci, std::shared_ptr<ConstantValueWrapper>(new ConstantValueWrapper(result)));
		return false;
	}

	// 1. true situation
	std::shared_ptr<AnalysisState> trueas = as->MakeCopy();
	if(trueas == NULL)
		return true;
	if(prd == CmpInst::Predicate::ICMP_EQ)
		trueas->MergeFakeValueRecord(opValueRecords[0], opValueRecords[1]);
	else if(prd == CmpInst::Predicate::ICMP_NE)
		trueas->RecordCMPResult(opValueRecords[0], opValueRecords[1], CmpInst::Predicate::ICMP_NE, true);
	trueas->AddLocalVariableRecord(ci, std::shared_ptr<ConstantValueWrapper>(new ConstantValueWrapper(trueResult)));

	// 2. false situation
	std::shared_ptr<AnalysisState> falseas = as->MakeCopy();
	if(falseas == NULL)
		return true;	
	if(prd == CmpInst::Predicate::ICMP_NE)
		falseas->MergeFakeValueRecord(opValueRecords[0], opValueRecords[1]);
	else if(prd == CmpInst::Predicate::ICMP_EQ)
		falseas->RecordCMPResult(opValueRecords[0], opValueRecords[1], CmpInst::Predicate::ICMP_EQ, false);
	falseas->AddLocalVariableRecord(ci, std::shared_ptr<ConstantValueWrapper>(new ConstantValueWrapper(falseResult)));

	globalContext->tpLock.lock();
	CreateAnalysisTask_NoLock(ci->getParent(), trueas, ci, CMP_True, thdStr);
	CreateAnalysisTask_NoLock(ci->getParent(), falseas, ci, CMP_False, thdStr);
	globalContext->tpLock.unlock();

	return true;
}

void UafDetectionPass::CreateAnalysisTask_NoLock(BasicBlock* bb,
		std::shared_ptr<AnalysisState> as, Instruction* si, ExecuteRelationship er, std::string thread){
	auto at = std::shared_ptr<AnalysisTask>(new AnalysisTask(bb, as, si, this, er, thread));
	todoTasks.push(at);
	return;
}

void UafDetectionPass::PrintThreadSum(UafDetectionPass* pass, std::string thdStr){
	if(!pass->globalContext->printDB)
		return;
	pass->globalContext->opLock.lock();
	OP << "[Tread-" << thdStr << "] " << "[DBG] THREAD FINISHED @ " << GetCurrentTime() << "\n";
	pass->globalContext->opLock.unlock();
	return;
}

bool UafDetectionPass::CheckConstantEqual(Constant* c1, Constant* c2){
	if(isa<ConstantPointerNull>(c1) && isa<ConstantPointerNull>(c2))
		return true;
	if(isa<ConstantPointerNull>(c1) && isa<ConstantInt>(c2))
		return dyn_cast<ConstantInt>(c2)->getZExtValue() == 0;
	if(isa<ConstantPointerNull>(c2) && isa<ConstantInt>(c1))
		return dyn_cast<ConstantInt>(c1)->getZExtValue() == 0;
	if(isa<ConstantInt>(c2) && isa<ConstantInt>(c1))
		return dyn_cast<ConstantInt>(c1)->getZExtValue() == dyn_cast<ConstantInt>(c2)->getZExtValue();
	return false;
}

UafDetectionPass::UafDetectionPass(std::string tm, bool isSingleThread, GlobalContext* gc)
  : IterativeModulePass(gc, "UAFDetection") {
  targetMehtod = tm;
  globalContext = gc;
  funcWrapper = new FunctionWrapper(globalContext);
  threadPoolLimit = std::thread::hardware_concurrency() * gc->thdPerCPU;
}

UafDetectionPass::UafDetectionPass(std::string tm1,
		std::string tm2, bool isSingleThread, GlobalContext* gc)
  : IterativeModulePass(gc, "UAFDetection") {
	tmS1 = tm1;
	tmS2 = tm2;
	globalContext = gc;
	funcWrapper = new FunctionWrapper(globalContext);
	threadPoolLimit = std::thread::hardware_concurrency() * gc->thdPerCPU;
}

UafDetectionPass::~UafDetectionPass(){
	delete funcWrapper;

	if(globalContext->dlHelper != NULL)
		delete globalContext->dlHelper;
	if(globalContext->ssm != NULL)
		delete globalContext->ssm;
}

void UafDetectionPass::GenerateArgMemBlock(Function* func, std::shared_ptr<CallRecord> callRecord, std::shared_ptr<AnalysisState> as){
	CallInst* callInst = callRecord->callInst;
	int count = 0;
	for(Function::arg_iterator argit = func->arg_begin(); argit != func->arg_end(); argit++, count++){
		Argument* arg = &*argit;

		if(callInst == NULL){
			if(isa<PointerType>(arg->getType())){
				std::shared_ptr<MemoryBlock> argMB(new MemoryBlock(arg, MBType::Arg, as->globalContext));
				as->AddLocalVariableRecord(arg, argMB);
			}
			else{
				std::shared_ptr<SymbolicValue> argSV(new SymbolicValue());
				as->AddLocalVariableRecord(arg, argSV);
			}
			continue;
		}

		Value* realArg = callInst->getArgOperand(count);
		if(isa<ConstantData>(realArg)){
			std::shared_ptr<ConstantValueWrapper> cvw(new ConstantValueWrapper(realArg));
			as->AddLocalVariableRecord(arg, cvw);
			continue;
		}

		std::shared_ptr<StoredElement> raVR = as->GetVariableRecord(realArg, 1);
		if(arg->hasByValOrInAllocaAttr()){
			std::shared_ptr<MemoryBlock> newmb(new MemoryBlock(arg, MBType::Arg, as->globalContext));
			as->AddLocalVariableRecord(arg, newmb);
			std::shared_ptr<MemoryBlock> ramb = std::dynamic_pointer_cast<MemoryBlock>(raVR);
			assert(ramb != NULL);
			
			Type* mbType = ramb->valueType;
			if(isa<StructType>(mbType) || isa<ArrayType>(mbType))
				newmb->CopyFields_NoLock(ramb, as);
			else{
				auto rv = as->GetMBContainedValue(ramb);
				as->RecordMBContainedValue(newmb, rv);
			}
		}
		else
			as->AddLocalVariableRecord(arg, raVR);
	}
}

std::string UafDetectionPass::GetBasicBlockLines(BasicBlock* bb){
	std::stringstream tmpSS;
	std::string funcname = bb->getParent()->getName();
	tmpSS << "(" << bb << ")" << funcname << " @ ";

 	std::set<int> lineNums;
	BasicBlock::InstListType& instList = bb->getInstList();
	for(BasicBlock::iterator instIT = instList.begin(); instIT != instList.end(); instIT++){
		Instruction* inst = &*instIT;
		if(!inst->hasMetadata())
			continue;

		DebugLoc dl = inst->getDebugLoc();
		if(dl.get() == NULL)
			continue;
		// TODO: wo don't need the following checks here, I think.
		// MDNode* mdn = dl.getScope();
		// if(DILexicalBlock* dilb = dyn_cast<DILexicalBlock>(mdn))
		// 	mdn = dilb->getScope();
		// if(mdn != scope)
		// 	continue;
		lineNums.insert(dl->getLine());
	}

	if(lineNums.size() > 0){
		std::set<int>::iterator it = lineNums.begin();
		int startNum = *it;
		int endNum = *it;
		it++;
		std::set<int>::iterator end = lineNums.end();
		while(it != end){
			if(*it == endNum + 1)
				endNum++;
			else{
				tmpSS << startNum;
				if(startNum != endNum)
					tmpSS << "-" << endNum;
				tmpSS << ",";
				startNum = endNum = *it;
			}
			it++;
		}
		tmpSS << startNum;
		if(startNum != endNum)
			tmpSS << "-" << endNum;
	}
	else
		tmpSS << "???";

  	return tmpSS.str();
}