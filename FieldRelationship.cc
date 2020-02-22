/*
 * FieldRelationship.cc
 *
 *  Created on: 2017年5月15日
 *      Author: kame
 */

#include "headers/FieldRelationship.h"
#include "llvm/IR/Constants.h"

FieldRelationship::~FieldRelationship(){
}

FieldRelationship::FieldRelationship(MemoryBlock* cmb, 
		std::shared_ptr<MemoryBlock> fmb,
		int64_t id, uint64_t sz):
		containerBlock(cmb), fieldBlock(fmb), size(sz){
	LLVMContext& llvmCtx = *cmb->globalContext->llvmContext;
	IntegerType* it64 = IntegerType::getInt64Ty(llvmCtx);
	ConstantInt* inCI = ConstantInt::getSigned(it64, id);
	indexVR = std::shared_ptr<ConstantValueWrapper>(new ConstantValueWrapper(inCI));
}

FieldRelationship::FieldRelationship(MemoryBlock* cmb,
		 std::shared_ptr<MemoryBlock> fmb,
		std::shared_ptr<StoredElement> id, uint64_t sz):
		containerBlock(cmb), fieldBlock(fmb), indexVR(id), size(sz)
{}

std::shared_ptr<MemoryBlock> FieldRelationship::GetFieldBlock(){
	return fieldBlock;
}

MemoryBlock* FieldRelationship::GetContainerBlock(){
	return containerBlock;
}

void FieldRelationship::SetContainerBlock(MemoryBlock* mb){
 containerBlock = mb;
}
