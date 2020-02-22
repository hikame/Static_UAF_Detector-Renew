/*
 * AnalysisState.cc
 *
 *  Created on: 2017年5月15日
 *      Author: kame
 */

#include "headers/AnalysisState.h"
#include "headers/Config.h"
#include "headers/Common.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Operator.h"
#include <map>

void AnalysisState::AddMemoryBlockTag(std::shared_ptr<MemoryBlock> mb, std::string name, void* value){
	std::shared_ptr<MemoryBlockTag> mbt(new MemoryBlockTag(name, value));
	mbTags.insert(std::pair<std::shared_ptr<MemoryBlock>, std::shared_ptr<MemoryBlockTag>>(mb, mbt));
	for(auto fr : mb->GetFields_NoLock())
		AddMemoryBlockTag(fr->fieldBlock, name, value);
}

void* AnalysisState::GetMemoryBlockTag(std::shared_ptr<MemoryBlock> mb, std::string name){
	for(std::pair<std::shared_ptr<MemoryBlock>, std::shared_ptr<MemoryBlockTag>> pair : mbTags){
		if(pair.first == mb){
			std::shared_ptr<MemoryBlockTag> tag = pair.second;
			if(tag->name == name)
				return tag->value;
		}
	}
	return NULL;
}

void AnalysisState::CopyMemoryBlockTags(std::shared_ptr<MemoryBlock> destMB, MemoryBlock* sourceMB){
	for(std::pair<std::shared_ptr<MemoryBlock>, std::shared_ptr<MemoryBlockTag>> pair : mbTags){
		if(pair.first.get() == sourceMB)
			mbTags.insert(std::pair<std::shared_ptr<MemoryBlock>, std::shared_ptr<MemoryBlockTag>>(destMB, pair.second));
	}
}

void AnalysisState::GenerateStackMB(AllocaInst* allocInst){
	std::shared_ptr<MemoryBlock> stackMB(new MemoryBlock(allocInst, MBType::Stack, globalContext));
	AddVariableRecord(allocInst, stackMB);
}

bool AnalysisState::ShouldNotContinue(BasicBlock* targetBB){
	std::list<std::shared_ptr<ExecutionRecord>>::iterator it = executionPath.end();
	unsigned anaCount = 0;
	it--;
	while(true){
		std::shared_ptr<ExecutionRecord> rc = *it;
		BasicBlock* bb = rc->bb;
		ExecuteRelationship er = rc->er;
		if(er == Return){ // this function has called some functions, omit their blocks
			int retCount = 1;
			while(true){
				it--;
				rc = *it;
				if(rc->er == Call)
					retCount--;
				else if(rc->er == Return)
					retCount++;
				if(retCount == 0)
					break;
			}
		}

		if(globalContext->LoopCheckSet.count(er)){
			if(bb == targetBB){
				anaCount++;
				if(anaCount >= globalContext->bbAnaLimit)
					return true;
			}
		}
		
		if(er == Call) // this is the first BB in this functions.
			return false;
		if(it == executionPath.begin())
			return false;
		it--;
	}
}

bool AnalysisState::IsLastAnalyzedBB(BasicBlock* bb){
	std::list<std::shared_ptr<ExecutionRecord>>::iterator lastit = executionPath.end();
	lastit--;
	lastit--;
	while((*lastit)->bb->getParent() != bb->getParent())
		lastit--;
	return ((*lastit)->bb == bb);
}

void AnalysisState::AddAnalyzedBasicBlock(BasicBlock* bb, ExecuteRelationship er){
	std::shared_ptr<ExecutionRecord> newER(new ExecutionRecord(bb, er));
	executionPath.push_back(newER);
	return;
}

std::shared_ptr<CallRecord> AnalysisState::AddNewCallRecord(CallInst* ci, Function* func){
	std::shared_ptr<CallRecord> newCR(new CallRecord(func, ci));
	callPath.push_back(newCR);
	analyzedFunctions.insert(func);
	return newCR;
}

bool AnalysisState::ShouldAnalyzeFunction(Function* func){
	size_t size = analyzedFunctions.count(func);
	if(size > globalContext->funcAnaLimit)
		return false;
	if(globalContext->ignoredFuncs.count(func->getName()) > 0)
		return false;
	return true;
}

CallInst* AnalysisState::TakeOutLastCall(){
	if(callPath.size() == 0)
		return NULL;
	std::shared_ptr<CallRecord> lastCR = callPath.back();
	lastCR->lock.lock();
	std::set<std::shared_ptr<MemoryBlock>> allocaMBs;
	for(auto bb = lastCR->func->getBasicBlockList().begin();
			bb != lastCR->func->getBasicBlockList().end();
			bb++){
		for(auto iit = bb->getInstList().begin(); iit != bb->getInstList().end(); iit++){
			Instruction* inst = &*iit;
			if(isa<AllocaInst>(inst)){
				std::shared_ptr<StoredElement> se = variableValues[inst];
				if(se == NULL)
					continue;
				std::shared_ptr<MemoryBlock> heapMB = std::dynamic_pointer_cast<MemoryBlock>(se);
				assert(heapMB != NULL);
				allocaMBs.insert(heapMB);
				variableValues.erase(inst);
				mbContainedValues.erase(heapMB);
			}
		}
	}
	lastCR->lock.unlock();

	// if some alloced memoryblock are still used by some StoredElement, we should mark these objects with free tags
	for(std::shared_ptr<MemoryBlock> mb : allocaMBs){
		size_t count = 0;
		for(auto v : variableValues)
			if(v.second == mb)
				count++;
		for(auto v : mbContainedValues)
			if(v.second == mb && allocaMBs.count(mb) == 0)  // some non-local memoryblock stored this memoryblock as its stored element
				count++;
		if(count != 0) {
			if(globalContext->printDB){
				Instruction* inst = dyn_cast<Instruction>(mb->GetBaseValue());
				std::lock_guard<std::mutex> lg(globalContext->opLock);
				OP << "[Tread-" << GetThreadID() << "] "
						<< "[DBG] [Free] Stack memory block based on " << globalContext->GetInstStr(inst) << " is freed by return instructions, "
						<< "while it's used a StoredElement by some value or other memorylbocks is " << count 
						<< " (" << inst->getParent() << "@" 
						<< inst->getParent()->getParent()->getName() << ").\n";
			}
			AddMemoryBlockTag(mb, "FreeInst", lastCR->callInst);
		}
	}

	// clear the mbTags of some memoryblocks which are no longer used by variables or other memoryblocks
	ClearUselessMBTags();

	CallInst* ret = lastCR->callInst;
	callPath.pop_back();
	return ret;
}

std::shared_ptr<CallRecord> AnalysisState::GetLastCall(){
	if(callPath.size() == 0)
		return NULL;
	std::shared_ptr<CallRecord> ret = callPath.back();
	return ret;
}

std::shared_ptr<AnalysisState> AnalysisState::MakeCopy(){
	// Check whether have enough system memory
	while(!globalContext->memAvaliable){
		sleep(1);
		if(globalContext->shouldQuit)
			return NULL;
	}

	// first of all, clear useless mbTags in this object's record
	ClearUselessMBTags();

	std::shared_ptr<AnalysisState> newAS = std::shared_ptr<AnalysisState>(new AnalysisState(llvmContext, globalContext));
	newAS->executionPath = executionPath;
	newAS->callPath = callPath;
	newAS->instCount = instCount;

	for(std::pair<Value*, std::shared_ptr<StoredElement>> pair : variableValues)
		newAS->AddVariableRecord(pair.first, pair.second);

	for(std::pair<std::shared_ptr<MemoryBlock>, std::shared_ptr<StoredElement>> pair : mbContainedValues)
		newAS->RecordMBContainedValue(pair.first, pair.second);

	newAS->cmpRecords = cmpRecords;
	for(auto mt : mbTags){
		if(mt.first.use_count() != mbTags.count(mt.first) * 2)  // clear useless mbTags
			newAS->mbTags.insert(mt);
	}
	return newAS;
}

AnalysisState::~AnalysisState(){
	if(globalContext->printDB){
		std::string thdStr = GetThreadID();
		globalContext->opLock.lock();
		OP << "[Tread-" << thdStr << "] [DBG] This Thread is goning to exits!\n";
		OP << "[Tread-" << thdStr << "] " << "[DBG] The number of the analyzed instructions are: " << instCount << ".\n";
		OP << "[Tread-" << thdStr << "] " << "[DBG] The number of the analyzed basic blocks are: " << executionPath.size() << ".\n";
		OP << "[Tread-" << thdStr << "] " << "[DBG] The number of the analyzed functions are: " << callPath.size() << ".\n";
		OP << "[Tread-" << thdStr << "] " << "[DBG] ### Summarization Finished ###\n";
		globalContext->opLock.unlock();
	}
}

void AnalysisState::MergeFakeValueRecord(std::shared_ptr<StoredElement> v1, std::shared_ptr<StoredElement> v2){
	if(std::dynamic_pointer_cast<SymbolicValue>(v1) != NULL){
		std::shared_ptr<StoredElement> tmp = v1;
		v1 = v2;
		v2 = tmp;	//v2 should always be the faked value, v1 may or may not be a faked value
	}

	std::set<Value*> todoValueList;
	for(std::pair<Value*, std::shared_ptr<StoredElement>> pair : variableValues)
		if(pair.second == v2)
			todoValueList.insert(pair.first);

	for(Value* todoV : todoValueList){
		AddVariableRecord(todoV, v1);
	}

	std::set<std::shared_ptr<MemoryBlock>> todoMBList;
	for(std::pair<std::shared_ptr<MemoryBlock>, std::shared_ptr<StoredElement>> pair : mbContainedValues)
		if(pair.second == v2)
			todoMBList.insert(pair.first);

	for(std::shared_ptr<MemoryBlock> todoMB : todoMBList)
		RecordMBContainedValue(todoMB, v1);
}

void AnalysisState::PrintExectutionPath(){
	std::string thdStr = GetThreadID();
	std::lock_guard<std::mutex> lg(globalContext->opLock);
	if(globalContext->printTC){
		OP << "[Tread-" << thdStr << "]" << " [EXP] ========= Current Execution Path. =========\n";
		for(std::list<std::shared_ptr<ExecutionRecord>>::iterator it = executionPath.begin();
			it != executionPath.end(); it++){
			ExecuteRelationship er = (*it)->er;
			auto bb = (*it)->bb;
			std::string erstr = GetERString(er);
			// std::string bbname = (*it)->bb->getName();
			std::string funcname = (*it)->bb->getParent()->getName();
			OP 	<< "[Tread-" << thdStr << "] [EXP] " 
				<< UafDetectionPass::GetBasicBlockLines(bb) 
				<< " - " << erstr << "\n";
		}
		OP << "[Tread-" << thdStr << "]" << " [EXP] ============================================\n";
	}
	else if(globalContext->printDB || globalContext->printWN || globalContext->printER){
		std::list<std::shared_ptr<ExecutionRecord>>::iterator it = executionPath.end();
		it--;
		ExecuteRelationship er = (*it)->er;
		auto bb = (*it)->bb;
		std::string erstr = GetERString(er);
		std::string funcname = (*it)->bb->getParent()->getName();
		OP 	<< "[Tread-" << thdStr << "] [EXP] " 
			<< UafDetectionPass::GetBasicBlockLines(bb) 
			<< " - " << erstr << "\n";
	}
}


void AnalysisState::PrepareForStep2(){
//TODO implement this function!
}

void AnalysisState::AddVariableRecord(Value* variable, std::shared_ptr<StoredElement> newValue){
	variableValues[variable] = newValue;
}

std::shared_ptr<StoredElement> AnalysisState::QueryVariableRecord(Value* variable){
	if(auto ele = variableValues[variable])
		return ele;

	if(Constant* constant = dyn_cast<Constant>(variable)){
		if(GlobalVariable* gVariable = dyn_cast<GlobalVariable>(variable)){
			Type* gvType = gVariable->getType();
			assert(isa<PointerType>(gvType));
			std::shared_ptr<MemoryBlock> mb(new MemoryBlock(gVariable, Global, globalContext));
			AddVariableRecord(gVariable, mb);

			if(globalContext->trustGlobalInit){
				if(gVariable->hasInitializer()){
					Constant* initV = gVariable->getInitializer();
					if(ConstantAggregate* ca = dyn_cast<ConstantAggregate>(initV)){
						unsigned num = ca->getNumOperands();
						for(int i = 0; i < num; i++){
							Constant* value = ca->getOperand(i);
							if(!isa<ConstantAggregateZero>(value)){
								uint64_t size = globalContext->dlHelper->GetTypeStoreSize(value->getType());
								auto fr = mb->FindFieldRelationship(i, size);
								assert(fr != NULL);
								std::shared_ptr<MemoryBlock> fmb = fr->GetFieldBlock();
								assert(fmb != NULL);
								RecordMBContainedValue(fmb, std::shared_ptr<ConstantValueWrapper>(new ConstantValueWrapper(value)));
							}
						}
					}
					else // we believe that the value can be used directly.
						RecordMBContainedValue(mb, std::shared_ptr<ConstantValueWrapper>(new ConstantValueWrapper(initV)));
				}// end of gVariable->hasInitializer()
				else if(globalContext->printWN){
					std::lock_guard<std::mutex> lg(globalContext->opLock);
					OP << "[Tread-" << GetThreadID() << "] [WRN] Global Variable - " << globalContext->GetInstStr(gVariable) << " has no initializer. \n";
				}
			} // end of trueGlobalInit
			else if(gVariable->hasInitializer() && isa<FunctionType>(gVariable->getInitializer()->getType())){
				if(globalContext->printDB){
					globalContext->opLock.lock();
					OP << "[Tread-" << GetThreadID() 
						<< "] [DBG] Although don't trust glboal initializer, trust global initializer (FunctionType) of " 
						<< globalContext->GetInstStr(gVariable) << ".\n";
					globalContext->opLock.unlock();
				}
				RecordMBContainedValue(mb, std::shared_ptr<ConstantValueWrapper>(new ConstantValueWrapper(gVariable->getInitializer())));
			}

			return mb;
		} // end of dyn_cast<GlobalVariable>
		if(isa<ConstantExpr>(variable))
			return NULL; // leave this for GetVariableRecord to handle
		// This is just a normal constant value.
		std::shared_ptr<ConstantValueWrapper> cvw(new ConstantValueWrapper(variable));
		variableValues[variable] = cvw;
		return cvw;
	}

	return NULL;
}

std::shared_ptr<StoredElement> AnalysisState::GetVariableRecord(Value* variable){
	if(auto qr = QueryVariableRecord(variable))
		return qr;

	if(ConstantExpr* ce = dyn_cast<ConstantExpr>(variable)){
		if(GEPOperator* gep = dyn_cast<GEPOperator>(variable)){
			// we don't provide newas set here. Therefore, the information 
			// of possible contained value are lost if the indexs in gep are symbolic
			return globalContext->UDP->AnalyzeGEPOperator(gep, this);  // this will AddVariableRecord if needed 
		}

		unsigned opcode = ce->getOpcode();
		if(opcode > Instruction::CastOpsBegin && opcode < Instruction::CastOpsEnd){
			Constant* conOpe0 = ce->getOperand(0);
			std::shared_ptr<StoredElement> vr = GetVariableRecord(conOpe0);
			AddVariableRecord(ce, vr);
			return vr;
		}
	}

	// auto generate fake value
	std::shared_ptr<SymbolicValue> sv(new SymbolicValue());
	AddVariableRecord(variable, sv);
	if(globalContext->printDB){
		std::lock_guard<std::mutex> lg(globalContext->opLock);
		OP << "[Tread-" << GetThreadID() << "] " <<
							"[DBG] Generate fake value for variable: " << globalContext->GetInstStr(variable) << "\n";
	}
	return sv;
}

void AnalysisState::RecordMBContainedValue(std::shared_ptr<MemoryBlock> mb, std::shared_ptr<StoredElement> newValue){
	mbContainedValues[mb] = newValue;
}

std::shared_ptr<StoredElement> AnalysisState::GetMBContainedValue(std::shared_ptr<MemoryBlock> mb, bool autoGenerateFV){
	std::map<std::shared_ptr<MemoryBlock>, std::shared_ptr<StoredElement>>::iterator it = mbContainedValues.find(mb);
	if(it == mbContainedValues.end()){
		if(autoGenerateFV || mb->isFake){
			std::shared_ptr<SymbolicValue> sv(new SymbolicValue());
			RecordMBContainedValue(mb, sv);
			if(globalContext->printDB){
				std::lock_guard<std::mutex> lg(globalContext->opLock);
				OP << "[Tread-" << GetThreadID() << "] " <<
					"[DBG] Generate fake value for memory block.\n";
			}
			return sv;
		}
		else
			return NULL;
	}
	return it->second;
}

void AnalysisState::RecordCMPResult(std::shared_ptr<StoredElement> v1, std::shared_ptr<StoredElement> v2,
		CmpInst::Predicate pred, bool result){
	std::shared_ptr<CMPRecord> cmpr(new CMPRecord(v1, v2, pred, result));
	cmpRecords.insert(cmpr);
}

Value* AnalysisState::QueryCMPRecord(std::shared_ptr<StoredElement> v1, 
		std::shared_ptr<StoredElement> v2, CmpInst::Predicate pred){
	ConstantInt* trueResult = ConstantInt::getTrue(*llvmContext);
	ConstantInt* falseResult = ConstantInt::getFalse(*llvmContext);
	Value* result = NULL;

	for(std::shared_ptr<CMPRecord> cr : cmpRecords){
		bool sameVR = false;
		if(v1 == cr->vr1 && v2 == cr->vr2)
			sameVR = true;
		if(v2 == cr->vr1 && v1 == cr->vr2)
			sameVR = true;
		if(!sameVR)
			continue;

		bool bresult = cr->result;
		if(pred != cr->pred)
			bresult = !bresult;

		result = bresult ? trueResult : falseResult;
	}
	return result;
}

CMPRecord::CMPRecord(std::shared_ptr<StoredElement> v1, std::shared_ptr<StoredElement> v2, CmpInst::Predicate pd, bool r){
	vr1 = v1;
	vr2 = v2;
	pred = pd;
	result = r;
}

CMPRecord::~CMPRecord(){
}

void AnalysisState::ClearUselessMBTags(){
	auto tag = mbTags.begin(); 
	while(tag != mbTags.end()){
		std::shared_ptr<MemoryBlock> mb = tag->first;
		size_t count = 0;
		for(auto v : variableValues)
			if(v.second == mb)
				count++;
		for(auto v : mbContainedValues)
			if(v.second == mb)  // some non-local memoryblock stored this memoryblock as its stored element
				count++;
		if(count == 0)
			tag = mbTags.erase(tag);
		else
			tag++;
	}
}
