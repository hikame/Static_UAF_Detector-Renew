/*
 * MemoryBlock.h
 *
 *  Created on: 2017年5月18日
 *      Author: kame
 */

#ifndef HEADERS_MEMORYBLOCK_H_
#define HEADERS_MEMORYBLOCK_H_

#include "AnalysisStructHeaders.h"

#define MemoryBlockSubclassID 234

enum MBType{
	Heap,
	Stack,
	Global,
	Field,
	Arg
};

/** 
 * MemoryBlock can be one type of StoredElement, because its address can be stored
 * by someone
*/
class MemoryBlock : public StoredElement{
	friend class FieldRelationship;
	DataLayoutHelper* dlHelper;
	Value* baseVariable = NULL;

	MBType memoryBlockType;
	uint64_t size = 0;
	// the container must be the direct container! 
	std::shared_ptr<FieldRelationship> containerFR = NULL; 
	std::set<std::shared_ptr<FieldRelationship>> fieldsFR; // some field memory block are not the direct field of this container, but the field of field.
	GlobalContext* globalContext;
public:
	std::mutex frLock;
	std::string baseVName;
	Type* valueType;
	bool isFake;

	MemoryBlock(Value* value, MBType tp, GlobalContext* gc, bool fakeStruct = false);
	MemoryBlock(MBType mbtp, GlobalContext* gc, uint64_t fieldSize, Type* fieldType, bool fakeStruct); // for field
	~MemoryBlock();

	MBType GetMemoryBlockType();
	Value* GetBaseValue();
	std::shared_ptr<FieldRelationship> GetContainerFR();
	void SetContainerRelationship(std::shared_ptr<FieldRelationship>);
	uint64_t GetSize();
	
	// Field relationship related
	// 1. will hold locks
	std::shared_ptr<FieldRelationship> FindFieldRelationship(int64_t index, 
		uint64_t size = 0);
	std::shared_ptr<FieldRelationship> FindFieldRelationship(std::shared_ptr<StoredElement> indexVR, uint64_t size);
	void AddNewFieldRelationship(std::shared_ptr<FieldRelationship>);
	std::shared_ptr<FieldRelationship> getField(AnalysisState* as, int64_t index);
	std::shared_ptr<FieldRelationship> getField(AnalysisState* as, 
		std::shared_ptr<SymbolicValue> index);

	// void ResetFieldsInRange(uint64_t start, uint64_t size, int64_t newValue, std::shared_ptr<AnalysisState> analysisState);

	// 2. without locks
	std::set<std::shared_ptr<FieldRelationship>>& GetFields_NoLock();
	void CopyFields_NoLock(
			std::shared_ptr<MemoryBlock> sourceMB, 
			std::shared_ptr<AnalysisState> analysisState);
	std::shared_ptr<FieldRelationship> FindFieldRelationship_NoLock(int64_t index, 
		uint64_t size = 0);
	std::shared_ptr<FieldRelationship> FindFieldRelationship_NoLock(std::shared_ptr<StoredElement> indexVR, uint64_t size = 0);
	void AddNewFieldRelationship_NoLock(std::shared_ptr<FieldRelationship>);
	void resetType(Type* type);
};

#endif /* HEADERS_MEMORYBLOCK_H_ */