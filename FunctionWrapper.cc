/*
 * FunctionWrapper.cc
 *
 *  Created on: 2017年5月27日
 *      Author: kame
 */

#include "headers/FunctionWrapper.h"
#include "llvm/IR/Constants.h"

FunctionWrapper::HandleResult FunctionWrapper::HandleFunction(Function* func, CallInst* ci, std::shared_ptr<AnalysisState> as){
	std::string funcName = func->getName().str();
	std::string thdStr = GetThreadID();
	if(as->globalContext->printDB){
		as->globalContext->opLock.lock();
		OP << "[Tread-" << thdStr << "] "
				<< "[DBG] Providing Wrapper for Handling Function: "
				<< funcName << "\n";
		as->globalContext->opLock.unlock();
	}
	if(CheckPrefix("llvm.memcpy", funcName)){
		Value* destValue = ci->getArgOperand(0);
		Value* sourceValue = ci->getArgOperand(1);
		Value* sizeValue = ci->getArgOperand(2);
		ConstantInt* sizeConst = dyn_cast<ConstantInt>(sizeValue);
		uint64_t size = 0;
		if(sizeConst == NULL){
			if(as->globalContext->printWN){
				as->globalContext->opLock.lock();
				OP << "[Tread-" << thdStr << "] "
						<< "[WRN] SIZE of Memcpy is not a constant int: " << as->globalContext->GetInstStr(sizeValue) << "\n";
				as->globalContext->opLock.unlock();
			}
			as->RecordWarn(Memcpy_with_Symbolic_Size);
		}else{
			size = sizeConst->getValue().getZExtValue();
			HandleMemcpy(sourceValue, destValue, size, as);
		}
		return FunctionWrapper::HandleResult::HasWrapper;
	}
	else if(CheckPrefix("llvm.memmove", funcName)){
		Value* destValue = ci->getArgOperand(0);
		Value* sourceValue = ci->getArgOperand(1);
		Value* sizeValue = ci->getArgOperand(2);
		ConstantInt* sizeConst = dyn_cast<ConstantInt>(sizeValue);
		uint64_t size = 0;
		if(sizeConst == NULL){
			if(as->globalContext->printWN){
				as->globalContext->opLock.lock();
				OP << "[Tread-" << thdStr << "] "
						<< "[WRN] SIZE of memmove is not a constant int: " << as->globalContext->GetInstStr(sizeValue);
				as->globalContext->opLock.unlock();
			}
			as->RecordWarn(Memmove_with_Symbolic_Size);
			//return;
		}else
			size = sizeConst->getValue().getZExtValue();

		HandleMemmove(sourceValue, destValue, size, as);
		return FunctionWrapper::HandleResult::HasWrapper;
	}

	else if(CheckPrefix("llvm.memset", funcName)){
		HandleMemset(ci, as, false);
		return FunctionWrapper::HandleResult::HasWrapper;
	}
	else if(funcName == "__memzero"){
		HandleMemset(ci, as, true);
		return FunctionWrapper::HandleResult::HasWrapper;
	}
	return FunctionWrapper::HandleResult::NoWrapper;
}

bool FunctionWrapper::CheckPrefix(std::string prefix, std::string funcname){
	auto res = std::mismatch(prefix.begin(), prefix.end(), funcname.begin());

	if(res.first == prefix.end())
		return true;
	return false;
}

bool FunctionWrapper::IsBasicField(MemoryBlock* mb){
	Type* mbType = mb->valueType;
	if(isa<StructType>(mbType) || isa<ArrayType>(mbType))
		return false;
	else
		return true;
}

/**
 * Get all the basic fields inside mb.
 * Arg mb has been checked not to be a basic field
 * return 0 on error happens
*/
size_t FunctionWrapper::GetBasicFieldsInRange(MemoryBlock* mb, uint64_t startpos, 
		uint64_t destsize, std::vector<std::shared_ptr<MemoryBlock>>& record,
		std::shared_ptr<AnalysisState> as){
	uint64_t mbsize = mb->GetSize();
	if(mbsize == 0)
		return 0;
	if(auto cf = mb->GetContainerFR()){
		if(cf->size != mbsize){
			if(globalContext->printER){
				std::string thdStr = GetThreadID();
				globalContext->opLock.lock();
				OP << "[Tread-" << thdStr << "] " << "[ERR] size of field memory block did not match: mb->GetSize() vs mb->GetContainerFR()->size: " << mbsize << " vs " << cf->size << "\n";
				globalContext->opLock.unlock();
			}
			return 0;
		}
	}

	if(mbsize - startpos >= destsize){ // the size of all fields in this mb is large enough
		// Get all the fields in [startpos, startpos + destsize)
		uint64_t pos = 0;
		if(StructType* strType = dyn_cast<StructType>(mb->valueType)){
			if(strType->isOpaque()){
				if(globalContext->printER){
					std::lock_guard<std::mutex> lg(globalContext->opLock);
					OP 	<< "[Tread-" << GetThreadID()
							<< "] [ERR] [GetBasicFieldsInRange] target struct is opaque." << "\n";
				}
				return 0;
			}

			std::map<int64_t, Type*> fieldTys;
			int64_t num = strType->getNumElements();
			for(uint64_t index = 0; index < num; index++){
				if(pos >= startpos + destsize)
					break;
				if(pos < startpos)
					continue;

				auto fieldFR = mb->getField(as.get(), index);
				// record all basic fields in this fieldBlock
				if(fieldFR == NULL || 
						GetBasicFieldsInRange(fieldFR->fieldBlock, 
							0, fieldFR->size, record, as) == 0)
					return 0;// some error hanppened here (such as destSize % eleSize != 0)
				pos = pos + fieldFR->size;
			}
		}
		else if(ArrayType* arrayType = dyn_cast<ArrayType>(mb->valueType)){
			Type* eleType = arrayType->getArrayElementType();
			uint64_t eleSize = globalContext->dlHelper->GetTypeStoreSize(eleType);
			if(destsize % eleSize != 0){
				if(globalContext->printWN){
					std::string thdStr = GetThreadID();
					globalContext->opLock.lock();
					OP << "[Tread-" << thdStr << "] " << "[WRN] [GetBasicFieldsInRange] destSize % eleSize != 0.";
					globalContext->opLock.unlock();
				}
				as->RecordWarn(GEP_using_Strange_Size);
				return 0;
			}
			uint64_t count = arrayType->getArrayNumElements();
			for(int64_t index = 0; index < count; index++){
				pos = eleSize * index;
				if(pos < startpos)
					continue;
				if(pos >= startpos + destsize)
					break;
				
				std::shared_ptr<FieldRelationship> eleFR = mb->getField(as.get(), index);
				// record all basic fields in this fieldBlock
				if(eleFR == NULL ||
						GetBasicFieldsInRange(eleFR->fieldBlock, 0, eleSize, record, as) == 0)
					return 0; // some error hanppened here (such as destSize % eleSize != 0)
			}
		}
		else
			assert(0); // this should never happened
	}
	else{ //ie: mbsize - startpos < destsize, this mb is not large enough, we need its container mb's object
		// (0) check if mb has container
		MemoryBlock* containerMB = NULL;
		auto containFR = mb->GetContainerFR();
		int64_t containindex = -1;

		if(containFR != NULL)
			containerMB = containFR->containerBlock;
		if(containerMB == NULL){
			if(globalContext->printWN){
				std::string thdStr = GetThreadID();
				globalContext->opLock.lock();
				OP << "[Tread-" << thdStr << "] " << "[WRN] current mb is not large enough but we cannot get its contaienr.";
				globalContext->opLock.unlock();
			}
			as->RecordWarn(GEP_without_Suitable_Container);
			return 0;
		}
		if(auto cvw = std::dynamic_pointer_cast<ConstantValueWrapper>(containFR->indexVR))
			if(ConstantInt* ci = dyn_cast<ConstantInt>(cvw->containedValue))
				containindex = ci->getSExtValue();
		
		if(containindex < 0){
			if(globalContext->printWN){
				std::string thdStr = GetThreadID();
				globalContext->opLock.lock();
				OP << "[Tread-" << thdStr << "] " << "[WRN] current mb is not large enough but its relationship with its container has a strange index value.";
				globalContext->opLock.unlock();
			}
			as->RecordWarn(GEP_without_Suitable_Container);
			return 0;
		}

		// (1) check the container memory blocks for more fields after this mb.
		// calculate the start pos for the container memory block
		auto cvt = containerMB->valueType;
		if(StructType* strType = dyn_cast<StructType>(cvt)){
			for(int64_t i = 0; i < containindex; i++){
				Type* fieldType = strType->getElementType(i);
				uint64_t fieldSize = globalContext->dlHelper->GetTypeStoreSize(fieldType);
				startpos = startpos + fieldSize;
			}
		}
		else if(ArrayType* arrayType = dyn_cast<ArrayType>(cvt))
			startpos = startpos + mbsize * containindex;
		else{
			globalContext->opLock.lock();
			OP << "[ERR] " << cvt->getTypeID() << "\n";
			globalContext->opLock.unlock();
			assert(0);  // unlikely to happen
		}

		if(GetBasicFieldsInRange(containerMB, startpos, destsize, record, as) == 0)
			return 0;
	}
	return record.size();
}

/** This function will not only do record but also do field finding*/
size_t FunctionWrapper::GetBasicFieldsInRange(std::shared_ptr<MemoryBlock> mb, 
		uint64_t startpos, uint64_t destsize, 
		std::vector<std::shared_ptr<MemoryBlock>>& record,
		std::shared_ptr<AnalysisState> as){
	uint64_t mbsize = mb->GetSize();
	if(mbsize == 0)
		return 0;
	// check mb is the a basic fields, if yes, record it and return
	if(IsBasicField(mb.get()) && mbsize == destsize){
		record.push_back(mb);
		return record.size();
	}
	return GetBasicFieldsInRange(mb.get(), startpos, destsize, record, as);
}

void FunctionWrapper::HandleMemcpy(Value* sourceValue, Value* destValue, uint64_t size, std::shared_ptr<AnalysisState> as){
	auto sourceVR = as->GetVariableRecord(sourceValue);
	auto destVR = as->GetVariableRecord(destValue);
	auto sourceMB = std::dynamic_pointer_cast<MemoryBlock>(sourceVR);
	auto destMB = std::dynamic_pointer_cast<MemoryBlock>(destVR);
	
	if(sourceMB == NULL){
		if(auto sv = std::dynamic_pointer_cast<SymbolicValue>(sourceVR)){
			sourceMB = std::shared_ptr<MemoryBlock>(
				new MemoryBlock(sourceValue, Heap, as->globalContext, true));
			as->MergeFakeValueRecord(sourceVR, sourceMB);
		}
		else{
			if(globalContext->printER){
				std::lock_guard<std::mutex> lg(globalContext->opLock);
				OP << "[Tread-" << GetThreadID() << 
					"] [ERR] Memcpy from a non-memoryblock.\n";
			}
			as->hasError = true;
			return;
		}
	}

	if(destMB == NULL){
		if(auto sv = std::dynamic_pointer_cast<SymbolicValue>(destVR)){
			destMB = std::shared_ptr<MemoryBlock>(
				new MemoryBlock(destValue, Heap, as->globalContext, true));
			as->MergeFakeValueRecord(destVR, destMB);
		}
		else{
			if(globalContext->printER){
				std::lock_guard<std::mutex> lg(globalContext->opLock);
				OP << "[Tread-" << GetThreadID() << 
					"] [ERR] Memcpy to a non-memoryblock.\n";
			}
			as->hasError = true;
			return;
		}
	}

	// get all the basic fields (fields that store basic data types) in range
	std::vector<std::shared_ptr<MemoryBlock>> sourceFields;
	std::vector<std::shared_ptr<MemoryBlock>> destFields;
	size_t sfs_count = GetBasicFieldsInRange(sourceMB, 0, size, sourceFields, as);
	size_t dfs_count = GetBasicFieldsInRange(destMB, 0, size, destFields, as);

	if(sfs_count == 0){
		if(as->globalContext->printWN){
			std::string thdStr = GetThreadID();
			as->globalContext->opLock.lock();
			OP << "[Tread-" << thdStr << "] " << "[WRN] cannot get all fields in the size range of source memoryblock.";
			as->globalContext->opLock.unlock();
		}
		as->RecordWarn(Memcpy_without_Suitable_Source);
		return;
	}
	if(dfs_count == 0){
		if(as->globalContext->printWN){
			std::string thdStr = GetThreadID();
			as->globalContext->opLock.lock();
			OP << "[Tread-" << thdStr << "] " << "[WRN] cannot get all fields in the size range of dest memoryblock.";
			as->globalContext->opLock.unlock();
		}
		as->RecordWarn(Memcpy_without_Suitable_Dest);
		return;
	} 
	if(sfs_count != dfs_count){
		if(as->globalContext->printWN){
			std::string thdStr = GetThreadID();
			as->globalContext->opLock.lock();
			OP << "[Tread-" << thdStr << "] " << "[WRN] we cannot handle the situation that the amount of source fields and dest fields mismatch...";
			as->globalContext->opLock.unlock();
		}
		as->RecordWarn(Memcpy_with_Mismatched_Source_Dest);
		return;
	}

	// do value copy now
	for(size_t i = 0; i < sfs_count; i++){
		auto smb = sourceFields[i];
		auto dmb = destFields[i];
		// GetSize() of smb and dmb will not return 0, becase we have checked while recording them
		if(smb->GetSize() != dmb->GetSize()){
			if(as->globalContext->printWN){
				std::string thdStr = GetThreadID();
				as->globalContext->opLock.lock();
				OP << "[Tread-" << thdStr << "] " << "[WRN] we cannot handle the situation that the size of source field and dest field mismatch...";
				as->globalContext->opLock.unlock();
			}
			as->RecordWarn(Memcpy_with_Mismatched_Source_Dest);
			return;
		}

		auto svr = as->GetMBContainedValue(smb);
		as->RecordMBContainedValue(dmb, svr);
	}
}

void FunctionWrapper::HandleMemset(CallInst* ci, std::shared_ptr<AnalysisState> as, bool isMemZero){
	Value* destValue = ci->getArgOperand(0);
	auto destVR = as->QueryVariableRecord(destValue);
	auto destMB =std::dynamic_pointer_cast<MemoryBlock>(destVR);
	if(destMB == NULL){
		if(as->globalContext->printWN){
			std::string thdStr = GetThreadID();
			as->globalContext->opLock.lock();
			OP << "[Tread-" << thdStr << "] " << "[WRN] Target of memset is not a memory block: " << destVR.get();
			as->globalContext->opLock.unlock();
		}
		as->RecordWarn(Memset_with_Strange_Target);
		return;
	}

	Value* sizeValue;
	std::shared_ptr<StoredElement> newValueOpe = NULL;
	if(isMemZero)
		sizeValue = ci->getArgOperand(1);
	else{
		sizeValue = ci->getArgOperand(2);
		Value* nv = ci->getArgOperand(1);
		newValueOpe = as->QueryVariableRecord(nv);
		assert(newValueOpe == NULL || std::dynamic_pointer_cast<ConstantValueWrapper>(newValueOpe));
	}

	ConstantInt* sizeConst = dyn_cast<ConstantInt>(sizeValue);
	uint64_t size = 0;
	if(sizeConst == NULL){
		if(as->globalContext->printWN){
			std::string thdStr = GetThreadID();
			as->globalContext->opLock.lock();
			OP << "[Tread-" << thdStr << "] " << "[WRN] SIZE of Memset is not a constant int: " <<
					as->globalContext->GetInstStr(sizeValue);
			as->globalContext->opLock.unlock();
		}
		as->RecordWarn(Memset_with_Symbolic_Size);
		return;
	}else
		size = sizeConst->getValue().getZExtValue();

	// destMB->ResetFieldsInRange(0, size, newValue, as);
	std::vector<std::shared_ptr<MemoryBlock>> destFields;
	size_t dfs_count = GetBasicFieldsInRange(destMB, 0, size, destFields, as);
	for(size_t i = 0; i < dfs_count; i++){
		auto dmb = destFields[i];
		Type* type = dmb->valueType;
		if(newValueOpe == NULL){  // new value is 0
			if(PointerType* pt = dyn_cast<PointerType>(type))
				newValueOpe = std::shared_ptr<ConstantValueWrapper>(new ConstantValueWrapper(ConstantPointerNull::get(pt)));
			else // try to get the cv as ConstantInt // if(IntegerType* it = dyn_cast<IntegerType>(type))
				newValueOpe = std::shared_ptr<ConstantValueWrapper>(new ConstantValueWrapper(ConstantInt::get(type, 0)));
			if(newValueOpe == NULL){
				if(as->globalContext->printWN){
					as->globalContext->opLock.lock();
					OP << "[Tread-" << GetThreadID() << "] " 
						<< "[WRN] Basic field type of memset is not a Integer or Pointer. "
						<< "We will treat it as a constant int"
						<<	as->globalContext->GetInstStr(ci);
					as->globalContext->opLock.unlock();
				}
				as->RecordWarn(Memset_with_Strange_Field);
				IntegerType* it = IntegerType::getIntNTy(*as->globalContext->llvmContext, dmb->GetSize() * 8 /*size in bits*/);
				newValueOpe = std::shared_ptr<ConstantValueWrapper>(new ConstantValueWrapper(ConstantInt::get(it, 0)));
			}
		}
		if(newValueOpe == NULL)
			return;
		as->RecordMBContainedValue(dmb, newValueOpe);
	}
	return;
}

void FunctionWrapper::HandleMemmove(Value* sourceValue, Value* destValue, uint64_t size, std::shared_ptr<AnalysisState> as){
	// we use memcpy handler to handle memmove, it seems ok
	HandleMemcpy(sourceValue, destValue, size, as);
}
