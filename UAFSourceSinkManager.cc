#include "headers/UAFSourceSinkManager.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>

using namespace llvm;

bool UAFSourceSinkManager::IsSource(Instruction* i){
  if (CallInst *ci = dyn_cast<CallInst>(i)){
  	Function *func;
  	if (!(func = ci->getCalledFunction())) {
  		Value* v = ci->getCalledValue()->stripPointerCasts();
  		if(!(func = dyn_cast<Function>(&*v)))
  			return false;
  	}
    if (globalContext->HeapAllocFuncs.count(func->getName().str())){
		if(globalContext->printDB){
			std::string thdStr = GetThreadID();
			globalContext->opLock.lock();
			OP << "[Tread-" << thdStr << "] " <<
					"============================== FOUND NEW SOURCE ==============================\n";
			OP << "[Tread-" << thdStr << "] " <<
					"[DBG] [Source] "<< globalContext->GetInstStr(ci) <<" $$ FROM $$ " <<ci->getParent()->getParent()->getName()<<"\n";
			globalContext->opLock.unlock();
		}
    	return true;
    }
  }
  return false;
}

bool UAFSourceSinkManager::IsSink(Instruction* inst, std::shared_ptr<AnalysisState> analysisState){
	bool ret = false;
	for(unsigned num = inst->getNumOperands(), i = 0; i < num; i ++){
		Value* value = inst->getOperand(i);
		bool shouldAnalyze = isa<Argument>(value) || isa<User>(value);
		if(!shouldAnalyze)
			continue;
		auto vr = analysisState->QueryVariableRecord(value);
		if(vr == NULL)
			continue;
		auto mb = std::dynamic_pointer_cast<MemoryBlock>(vr);
		if(mb == NULL)
			continue;
		void* tag = analysisState->GetMemoryBlockTag(mb, "FreeInst");//mb->GetTag("FreeInst");
		if(tag == NULL)
			continue;

		if(StoreInst* si = dyn_cast<StoreInst>(inst)){
			if(si->getPointerOperand() == value)
				ret = true;
		}else if(LoadInst* li = dyn_cast<LoadInst>(inst)){
			if(li->getPointerOperand() == value)
				ret = true;
		}else if(CallInst* ci = dyn_cast<CallInst>(inst)){
			Function *func;
			if (!(func = ci->getCalledFunction())) {
				Value* v = ci->getCalledValue()->stripPointerCasts();
				if(!(func = dyn_cast<Function>(&*v))){
					auto record = analysisState->QueryVariableRecord(v);
					if(record == NULL)
						continue;
					auto cvw = std::dynamic_pointer_cast<ConstantValueWrapper>(record);
					if(cvw == NULL)
						continue;
					if(Function* tmp = dyn_cast<Function>(cvw->containedValue))
						func = tmp;
					else{
						if(analysisState->globalContext->printWN){
							analysisState->globalContext->opLock.lock();
							OP << "[Tread-" << GetThreadID() << "] "
								<< "[WRN] We cannot get the called function:"
								<< globalContext->GetInstStr(ci) << ".\n";
							analysisState->globalContext->opLock.unlock();
						}
						analysisState->RecordWarn(Call_without_Known_Target_Function);
					}
				}
			}
			if(func == NULL)
				ret = true;
			else if(func->getBasicBlockList().empty()){
				ret = true;		
			}
		}else if(GetElementPtrInst* gepInst = dyn_cast<GetElementPtrInst>(inst)){
			if(gepInst->getPointerOperand() == value){
				ret = true;
			}
		}

		if(ret == true){
			analysisState->AddMemoryBlockTag(mb, "SinkInst", inst);
			std::list<std::shared_ptr<ExecutionRecord>>::iterator it = 
				analysisState->executionPath.end();
			it--;
			std::shared_ptr<ExecutionRecord> er = *it;
			er->tag.insert(Reuse);

			// std::shared_ptr<MemoryBlock> containerMB = std::shared_ptr<MemoryBlock>(vr, mb);
			// RecordSinkPath(containerMB, inst, analysisState);
			RecordSinkPath(mb, inst, analysisState);

			if(globalContext->printDB){
				std::string thdStr = GetThreadID();
				globalContext->opLock.lock();
				OP << "[Tread-" << thdStr << "]" <<
						" ============================== FOUND SINK ==============================\n";
				if(mb->isFake || mb->GetBaseValue() == NULL)
					OP << "[Tread-" << thdStr << "] @ [" << GetCurrentTime()  <<
							"] [DBG] [Sink] Find Sink: A Fake Memory " << " is sunk by instruction "
								<< globalContext->GetInstStr(inst) << "\n";
				else
					OP << "[Tread-" << thdStr << "] @ [" << GetCurrentTime()  <<
							"] [DBG] [Sink] Find Sink: Memory Based on " << *mb->GetBaseValue()
							<< " is sunk by instruction " << globalContext->GetInstStr(inst) << "\n";
				globalContext->opLock.unlock();
			}
			break;
		}
	}
	return ret;
}

int UAFSourceSinkManager::AnalyzeTag(Instruction* inst, std::shared_ptr<AnalysisState> analysisState){
  CallInst *ci = dyn_cast<CallInst>(inst);
  if(ci == NULL)
  	return 1;

	Function *func;
	if (!(func = ci->getCalledFunction())) {
		Value* v = ci->getCalledValue()->stripPointerCasts();
		if(!(func = dyn_cast<Function>(&*v)))
			return 1;
	}
	std::string funcname = func->getName().str();
	bool isFree = (funcname == "free" || funcname == "kfree");
	bool isCacheFree = (funcname == "kmem_cache_free");
	if (isFree || isCacheFree){
		Value* fvalue = NULL;
		if(isFree)
			fvalue = ci->getArgOperand(0);
		else if(isCacheFree)
			fvalue = ci->getArgOperand(1);
		auto fvr = analysisState->GetVariableRecord(fvalue);
		auto fmb = std::dynamic_pointer_cast<MemoryBlock>(fvr);
		if(fmb == NULL){
			if(PointerType* pt = dyn_cast<PointerType>(fvalue->getType())){
				auto ctp = pt->getElementType();
				auto size = globalContext->dlHelper->GetTypeStoreSize(ctp);
				fmb = std::shared_ptr<MemoryBlock>(new MemoryBlock(Heap, globalContext, size, ctp, true));
				if(auto fsv = std::dynamic_pointer_cast<SymbolicValue>(fvr))
					analysisState->MergeFakeValueRecord(fmb, fsv);
			}
		}
		if(fmb == NULL){
			if(globalContext->printER){
				auto fcv = std::dynamic_pointer_cast<ConstantValueWrapper>(fvr);
				ConstantPointerNull* np = dyn_cast<ConstantPointerNull>(fcv->containedValue);
				std::lock_guard<std::mutex> lg(analysisState->globalContext->opLock);
				if(np)
					OP << "[Tread-" << GetThreadID() << "] " <<
						"[ERR] [Free] Use free on a NULL pointer: "
							<< analysisState->globalContext->GetInstStr(inst) << "\n";
				else
					OP << "[Tread-" << GetThreadID() << "] " <<
						"[ERR] [Free] Use a non-memory pointer to do free: "
							<< analysisState->globalContext->GetInstStr(inst) << "\n";
			}
			return -1;
		}

		if(globalContext->printDB){
			std::lock_guard<std::mutex> lg(analysisState->globalContext->opLock);
			if(fmb->isFake || fmb->GetBaseValue() == NULL){
				OP << "[Tread-" << GetThreadID() << "] " <<
						"[DBG] [Free] A Fake Memory is Freed by " << analysisState->globalContext->GetInstStr(inst) << "\n";
			}
			else if(Instruction* baseInst = dyn_cast<Instruction>(fmb->GetBaseValue())){
				OP << "[Tread-" << GetThreadID() << "] " <<
					"[DBG] [Free] Memory Based on "
						<< analysisState->globalContext->GetInstStr(baseInst) << " is Freed by "
						<< analysisState->globalContext->GetInstStr(inst) << "\n";
			}
			OP << "[Tread-" << GetThreadID() << "] " <<
					"[DBG] [Free] Current Execute Path is: \n";
			analysisState->PrintExectutionPath(true, false);
		}

		analysisState->AddMemoryBlockTag(fmb, "FreeInst", ci);
		std::list<std::shared_ptr<ExecutionRecord>>::iterator it = 
			analysisState->executionPath.end();
		it--;
		std::shared_ptr<ExecutionRecord> er = *it;
		er->tag.insert(Free);
		return 0;
  }
	return 1;
}

void UAFSourceSinkManager::CheckHeapUAF(std::shared_ptr<AnalysisState> as, MemoryBlock* memoryBlock, CallRecord* lastCR){

}

SinkRecord* UAFSourceSinkManager::RecordSinkPath(std::shared_ptr<MemoryBlock> mb,
		Instruction* sinkInst, std::shared_ptr<AnalysisState> analysisState){
	Instruction* freeInst = (Instruction*)analysisState->GetMemoryBlockTag(mb, "FreeInst");
	Instruction* sourceInst = NULL;
	if(Value* baseValue = mb->GetBaseValue())
		if(Instruction* inst = dyn_cast<Instruction>(baseValue))
			sourceInst = inst;

	SinkRecord* targetSR = NULL;
	globalContext->sinkRecordsLock.lock();
	for(SinkRecord* sr : sinkRecords){
		if(sr->sourceInst == sourceInst && sr->freeInst == freeInst && sr->sinkInst == sinkInst){
			targetSR = sr;
			break;
		}
	}
	bool firstRecord = false;
	if(targetSR == NULL){
		targetSR = new SinkRecord(mb, sinkInst, freeInst, globalContext, analysisState);
		sinkRecords.insert(targetSR);
		firstRecord = true;
	}
	globalContext->sinkRecordsLock.unlock();

	targetSR->HandleNewPath(analysisState->executionPath, firstRecord);
	return targetSR;
}

unsigned UAFSourceSinkManager::getArgNo(Value* tv, CallInst* ci){
  unsigned ArgNo = 0;
  CallSite CS(ci);
  for (CallSite::arg_iterator ai = CS.arg_begin(), ae = CS.arg_end();
       ai != ae; ++ai) {
    Value* arg = (Value*)ai;
    if (arg == tv)
      break;
    ++ArgNo;
  }

  return ArgNo;
}

UAFSourceSinkManager::UAFSourceSinkManager(GlobalContext *gc){
  globalContext = gc;
}
