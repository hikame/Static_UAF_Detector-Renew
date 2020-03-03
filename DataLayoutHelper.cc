/*
 * DataLayoutHelper.cc
 *
 *  Created on: 2017年6月22日
 *      Author: kame
 */
#include "headers/DataLayoutHelper.h"
#include "llvm/IR/Operator.h"

DataLayoutHelper::DataLayoutHelper(const DataLayout* dl, std::mutex *lk){
	DL = dl;
//	lock = lk;
	globalContext = NULL;
}

DataLayoutHelper::DataLayoutHelper(const DataLayout* dl, GlobalContext * gc){
	DL = dl;
//	lock = new std::mutex();
	globalContext = gc;
}

DataLayoutHelper::~DataLayoutHelper(){
//	delete lock;
}

uint64_t DataLayoutHelper::GetElementOffsetInStruct(StructType *Ty, unsigned index){
	std::pair<StructType *, int> pair = std::pair<StructType *, int>(Ty, index);
	std::lock_guard<std::mutex> lg(eorLock);
	std::map<std::pair<StructType *, int>, uint64_t>::iterator it =
			elementOffsetRecord.find(pair);
	if(it != elementOffsetRecord.end()){
		return it->second;
	}

	const StructLayout* strLayout = NULL;
	uint64_t ret = 0;
	strLayout = DL->getStructLayout(Ty);
	ret = strLayout->getElementOffset(index);
	elementOffsetRecord[pair] = ret;
	return ret;
}
// size in byte
uint64_t DataLayoutHelper::GetTypeStoreSize(Type *Ty){
	uint64_t ret = 0;
	if(!Ty->isSized())
		return 0;

	std::lock_guard<std::mutex> lg(ssrLock);
	std::map<Type*, uint64_t>::iterator it = storeSizeRecord.find(Ty);
	if(it != storeSizeRecord.end())
		return it->second;
	
	ret = DL->getTypeStoreSize(Ty);
	storeSizeRecord[Ty] = ret;
	return ret;
}

//void DataLayoutHelper::GetFieldTypes(StructType* strType, std::set<Type*>* results){
////	lock.lock();
//	for(StructType::element_iterator it = strType->element_begin();
//			it != strType->element_end();
//			it ++)
//		results->insert(*it);
////	lock.unlock();
//}

// Given a GEP insn or GEP const expr, compute its byte-offset
// The function will resolve nested GEP constexpr, but will not
// resolve nested GEP instruction.
int64_t DataLayoutHelper::GetGEPOffset(Value* value){
	gorLock.lock();
	std::map<Value*, uint64_t>::iterator it =
			gepOffsetRecord.find(value);
	if(it != gepOffsetRecord.end()){
		gorLock.unlock();
		return it->second;
	}
	gorLock.unlock();

  // Assume this function always receives GEP value.
  const GEPOperator* gepValue = dyn_cast<GEPOperator>(value);
  assert(gepValue != NULL &&
      "getGEPOffset receives a non-gep value!");

  int64_t offset = 0;
  const Value* baseValue =
    gepValue->getPointerOperand()->stripPointerCasts();
  // If we have yet another nested GEP const expr, accumulate its
  // offset. The reason why we don't accumulate nested GEP
  // instruction's offset is that we aren't required to. Also, it
  // is impossible to do that because we are not sure if the
  // indices of a GEP instruction contains all-consts or not.
  if (const ConstantExpr* cexp = dyn_cast<ConstantExpr>(baseValue))
    if (cexp->getOpcode() == Instruction::GetElementPtr)
      offset += GetGEPOffset((Value*)cexp);

  Type* ptrTy = gepValue->getPointerOperand()->getType();
  SmallVector<Value*, 4> indexOps(gepValue->op_begin() + 1,
      gepValue->op_end());
  // Make sure all indices are constants.
  for (unsigned i = 0, e = indexOps.size(); i != e; ++i)
  {
    if (!isa<ConstantInt>(indexOps[i]))
      indexOps[i] =
        ConstantInt::get(Type::getInt32Ty(ptrTy->getContext()), 0);
  }
  Type* srcElementTy = gepValue->getSourceElementType();
  gorLock.lock();
  offset += DL->getIndexedOffsetInType(srcElementTy, indexOps);
  gorLock.unlock();
  gepOffsetRecord[value] = offset;
  return offset;
}
