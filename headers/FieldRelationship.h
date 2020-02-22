#ifndef _FIELDRELATIONSHIP_H
#define _FIELDRELATIONSHIP_H

#include "AnalysisStructHeaders.h"

//using namespace llvm;

class FieldRelationship{
	friend class MemoryBlock;
public:
	// we don't want containerBlock to use shared_ptr here,
	// otherwise, the containerBlock will always hold a shared_ptr
	// of itself, and will never be released.
	MemoryBlock* containerBlock;
	std::shared_ptr<MemoryBlock> fieldBlock;
	// Actually, StoredElement can be ConstantValueWrapper or SymbolicValue 
	// (not MemoryBlock)
	std::shared_ptr<StoredElement> indexVR;
	uint64_t size = 0;
	~FieldRelationship();

	FieldRelationship(MemoryBlock* containerMB, std::shared_ptr<MemoryBlock> fieldMB, int64_t index, uint64_t size);
	FieldRelationship(MemoryBlock* containerMB, std::shared_ptr<MemoryBlock> fieldMB, std::shared_ptr<StoredElement> indexVR, uint64_t size);

	std::shared_ptr<MemoryBlock> GetFieldBlock();
  	MemoryBlock* GetContainerBlock();
  	void SetContainerBlock(MemoryBlock*);
};

#endif
