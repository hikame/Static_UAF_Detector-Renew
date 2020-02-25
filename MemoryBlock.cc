/*
 * MemoryBlock.cc
 *
 *  Created on: 2017年5月15日
 *      Author: kame
 */

#include "headers/MemoryBlock.h"
#include "llvm/IR/Constants.h"

std::shared_ptr<FieldRelationship> MemoryBlock::FindFieldRelationship_NoLock(
		int64_t index, uint64_t size){
	for(std::shared_ptr<FieldRelationship> fr : fieldsFR){
		if(auto cvw = std::dynamic_pointer_cast<ConstantValueWrapper>(fr->indexVR))
			if(ConstantInt* ci = dyn_cast<ConstantInt>(cvw->containedValue))
				if(ci->getSExtValue() == index)
					if(size == 0 || fr->size == size)
						return fr;
	}
	return NULL;
}

std::shared_ptr<FieldRelationship> MemoryBlock::FindFieldRelationship(
		int64_t index, uint64_t size){
	std::lock_guard<std::mutex> lg(frLock);
	return FindFieldRelationship_NoLock(index, size);
}

std::shared_ptr<FieldRelationship> MemoryBlock::FindFieldRelationship_NoLock(
		std::shared_ptr<StoredElement> indexVR, uint64_t size){
	if(auto indexSV = std::dynamic_pointer_cast<SymbolicValue>(indexVR)){
		for(std::shared_ptr<FieldRelationship> fr : fieldsFR){
			bool sizeOK = (size == 0 || fr->size == size);
			if(sizeOK && (fr->indexVR == indexVR))
				return fr;
		}
	}
	else{
		auto indexCV = std::dynamic_pointer_cast<ConstantValueWrapper>(indexVR);
		assert(indexCV != NULL);
		ConstantInt* qivr = dyn_cast<ConstantInt>(indexCV->containedValue);
		for(std::shared_ptr<FieldRelationship> fr : fieldsFR){
			bool sizeOK = (size == 0 || fr->size == size);
			if(sizeOK && (fr->indexVR == indexVR))
				return fr;
			if(auto fcv = std::dynamic_pointer_cast<ConstantValueWrapper>(fr->indexVR)){
				ConstantInt* fivr = dyn_cast<ConstantInt>(fcv->containedValue);
				if(fivr != NULL && qivr != NULL 
						&& fivr->getSExtValue( ) == qivr->getSExtValue())
					return fr;
			}
		}
	}
	return NULL;
}

std::shared_ptr<FieldRelationship> MemoryBlock::FindFieldRelationship(
		std::shared_ptr<StoredElement> indexVR, uint64_t size){
	std::lock_guard<std::mutex> lg(frLock);
	return FindFieldRelationship_NoLock(indexVR, size);
}

std::set<std::shared_ptr<FieldRelationship>>& MemoryBlock::GetFields_NoLock(){
	return fieldsFR;
}

/* When the function is called, `this` object must not be used by different threads
 * The copy operation includes field relationship and the contained values in them
*/
void MemoryBlock::CopyFields_NoLock(std::shared_ptr<MemoryBlock> sourceMB, std::shared_ptr<AnalysisState> analysisState){
	std::lock_guard<std::mutex> lg(sourceMB->frLock);	// in case of other thread operate on sourceMB
														// no need to lock this->frLock, because no one else use `this` object
	std::set<std::shared_ptr<FieldRelationship>>& sourceFields = sourceMB->GetFields_NoLock();
	for(std::shared_ptr<FieldRelationship> sfr : sourceFields){
		std::shared_ptr<MemoryBlock> sfmb = sfr->GetFieldBlock();
		std::shared_ptr<MemoryBlock> dfmb = NULL;

		std::shared_ptr<FieldRelationship> dfr = FindFieldRelationship(sfr->indexVR, sfr->size);
		if(dfr == NULL){
			dfmb = std::shared_ptr<MemoryBlock>(new MemoryBlock(Field, globalContext, sfmb->size, sfmb->valueType, sfmb->isFake));
			dfr = std::shared_ptr<FieldRelationship>(new FieldRelationship(this, dfmb, sfr->indexVR, sfr->size));
			AddNewFieldRelationship(dfr);
			dfmb->SetContainerRelationship(dfr);
		}
		else
			dfmb = dfr->GetFieldBlock();
		if(isa<StructType>(sfmb->valueType))
			dfmb->CopyFields_NoLock(sfmb, analysisState);
		else{
			auto sourceFieldCVR = analysisState->GetMBContainedValue(sfmb, false);
			if(sourceFieldCVR != NULL)
				analysisState->RecordMBContainedValue(dfmb, sourceFieldCVR);
		}
	}
}

MemoryBlock::MemoryBlock(Value* value, MBType tp, GlobalContext* gc, bool fake):
		baseVariable(value), memoryBlockType(tp){
	assert(tp != Field);
	globalContext = gc;
	isFake = fake;
	dlHelper = globalContext->dlHelper;
	assert(value != NULL);

	baseVName = value->getName().str();
	PointerType* pType = dyn_cast<PointerType>(value->getType());
	assert(pType != NULL);
	valueType = pType->getElementType();

	// handle size
	if(tp == Heap && !isFake){
		valueType = pType->getElementType();
		CallInst* ci = dyn_cast<CallInst>(value);
		Value* sizeValue = ci->getArgOperand(0);
		
		if(ConstantInt* cint = dyn_cast<ConstantInt>(sizeValue)){
			size = cint->getValue().getZExtValue();
			uint64_t eleSize = dlHelper->GetTypeStoreSize(valueType);
			uint64_t num = size / eleSize;
			valueType = ArrayType::get(valueType, num);
		}
	}
	else{
		if(valueType->isSized())
			size = dlHelper->GetTypeStoreSize(valueType);
	}
}

MemoryBlock::MemoryBlock(MBType mbtp, GlobalContext* gc,
		uint64_t fieldSize, Type* fieldType, bool fakeStruct){
	globalContext = gc;
	isFake = fakeStruct;
	memoryBlockType = mbtp;
	dlHelper = gc->dlHelper;
	size = fieldSize;
	valueType = fieldType;
}

MemoryBlock::~MemoryBlock(){
	// in case some contained memory blocks are not freed
	for(std::shared_ptr<FieldRelationship> fr : fieldsFR)
		fr->fieldBlock->containerFR = NULL; 
}

std::shared_ptr<FieldRelationship> MemoryBlock::GetContainerFR(){
	return containerFR;
}

void MemoryBlock::SetContainerRelationship(std::shared_ptr<FieldRelationship> fr){
	containerFR = fr;
}

void MemoryBlock::AddNewFieldRelationship_NoLock(std::shared_ptr<FieldRelationship> fr){
	fieldsFR.insert(fr);
// todo delete debug
if(fr->fieldBlock->GetSize() == this->GetSize() && fr->fieldBlock->valueType == this->valueType){
	OP << "";
}
}

void MemoryBlock::AddNewFieldRelationship(std::shared_ptr<FieldRelationship> fr){
	std::lock_guard<std::mutex> lg(frLock);
	fieldsFR.insert(fr);
// todo delete debug
if(fr->fieldBlock->GetSize() == this->GetSize() && fr->fieldBlock->valueType == this->valueType){
	OP << "";
}
}

Value* MemoryBlock::GetBaseValue(){
	return baseVariable;
}

// int MemoryBlock::GetFieldsCount(){
// 	return fieldsFR.size();
// }
// size in bytes
uint64_t MemoryBlock::GetSize(){
	if(size == 0){
		if(globalContext->printWN){
			globalContext->opLock.lock();
			OP << "[Tread-" << GetThreadID() << "] "
					<< "[WRN] size of memoryblock cann't be determinded: " << (baseVariable != NULL ? globalContext->GetInstStr(baseVariable) : "NULL");
			globalContext->opLock.unlock();
		}
	}
	return size;
}

MBType MemoryBlock::GetMemoryBlockType(){
	return memoryBlockType;
}


std::shared_ptr<FieldRelationship> MemoryBlock::getField(AnalysisState* as, int64_t index){
	std::lock_guard<std::mutex> lg(frLock);
	auto find = FindFieldRelationship_NoLock(index);
	if(find)
		return find;

	Type* eleType = NULL;
	int64_t eleSize = 0;
	if(StructType* strType = dyn_cast<StructType>(valueType)){
		if(strType->isOpaque()){
			if(globalContext->printER){
				std::lock_guard<std::mutex> lg(globalContext->opLock);
				OP 	<< "[Tread-" << GetThreadID()
						<< "] [ERR] Try to get field from a opaque type: "
						<< strType->getName() << "\n";
			}
			return NULL;
		}
		else if(index >= strType->getNumElements()){
			if(globalContext->printER){
				std::lock_guard<std::mutex> lg(globalContext->opLock);
				OP 	<< "[Tread-" << GetThreadID()
						<< "] [ERR] Get element from a strange type (index >= strType->getNumElements()): "
						<< strType->getName() << "\n";
			}
			return NULL;
		}
		else
			eleType = strType->getElementType(index);
	}
	else if(ArrayType* arrType = dyn_cast<ArrayType>(valueType))
		eleType = arrType->getArrayElementType();
	else if(PointerType* ptType = dyn_cast<PointerType>(valueType)){
		if(globalContext->printWN){
			std::lock_guard<std::mutex> lg(globalContext->opLock);
			OP 	<< "[Tread-" << GetThreadID()
					<< "] [ERR] find field from a pointer is a strange operation...";
		}
		as->RecordWarn(GEP_from_Pointer);
		eleType = ptType->getElementType();
		assert(index == 0);  // such as: %24 = getelementptr inbounds i8*, i8** %21, i64 %23 (%23 should be 0)
	}
	else{ // this may happen if some type cast happens while function calls
		if(globalContext->printWN){
			std::lock_guard<std::mutex> lg(globalContext->opLock);
			OP 	<< "[Tread-" << GetThreadID()
					<< "] [ERR] find field from a memoryblock which is not a array or struct or even a pointer...";  
		}
		as->RecordWarn(GEP_from_Strange_Type);
 		return NULL;
	}

	eleSize = globalContext->dlHelper->GetTypeStoreSize(eleType);
	std::shared_ptr<MemoryBlock> fmb(new MemoryBlock(Field, this->globalContext, eleSize, eleType, this->isFake));
	as->CopyMemoryBlockTags(fmb, this);
	auto fr = std::shared_ptr<FieldRelationship>(new FieldRelationship(this, fmb, index, eleSize));
	AddNewFieldRelationship_NoLock(fr);
	fmb->SetContainerRelationship(fr);
	return fr;
}

std::shared_ptr<FieldRelationship> MemoryBlock::getField(AnalysisState* as, 
		std::shared_ptr<SymbolicValue> index){
	std::lock_guard<std::mutex> lg(frLock);
	auto find = FindFieldRelationship_NoLock(index);
	if(find)
		return find;
	ArrayType* arrType = dyn_cast<ArrayType>(valueType);
	assert(arrType); // we cannot imagine any other types having element with symbolic index
	auto eleType = arrType->getArrayElementType();
	auto eleSize = globalContext->dlHelper->GetTypeStoreSize(eleType);
	std::shared_ptr<MemoryBlock> sfmb(new MemoryBlock(Field, this->globalContext, eleSize, eleType, this->isFake));
	auto fr = std::shared_ptr<FieldRelationship>(
		new FieldRelationship(this, sfmb, index, eleSize));
	AddNewFieldRelationship_NoLock(fr);
	sfmb->SetContainerRelationship(fr);
	return fr;
}

void MemoryBlock::resetType(Type* type){
	valueType = type;
	if(valueType->isSized())
		size = dlHelper->GetTypeStoreSize(valueType);

	// clear up the fields
	frLock.lock();
	for(std::shared_ptr<FieldRelationship> fr : fieldsFR)
		fr->fieldBlock->containerFR = NULL; 
	fieldsFR.clear();   // for fields currently recorded, clear them.
	frLock.unlock();
}