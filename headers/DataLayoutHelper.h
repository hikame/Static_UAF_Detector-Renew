/*
 * DataLayoutHelper.h
 *
 *  Created on: 2017年6月22日
 *      Author: kame
 */

#ifndef HEADERS_DATALAYOUTHELPER_H_
#define HEADERS_DATALAYOUTHELPER_H_

#include <llvm/IR/DataLayout.h>
#include "AnalysisStructHeaders.h"

class DataLayoutHelper{
	const DataLayout* DL;
	GlobalContext * globalContext;
	std::mutex ssrLock;
	std::mutex lock;
//	uint64_t lockcount = 0;

	std::map<Type*, uint64_t> storeSizeRecord;
	std::map<Value*, uint64_t> gepOffsetRecord;
	std::mutex gorLock;
	std::map<std::pair<StructType *, int>, uint64_t> elementOffsetRecord;
	std::mutex eorLock;
public:
	DataLayoutHelper(const DataLayout* dl, std::mutex *);
	DataLayoutHelper(const DataLayout* dl, GlobalContext *);
	~DataLayoutHelper();
	uint64_t GetTypeStoreSize(Type *Ty);
	uint64_t GetElementOffsetInStruct(StructType *Ty, unsigned index);
	int64_t GetGEPOffset(Value* value);
//	int64_t GetGEPOffset(GEPOperator* gep);
//	void GetFieldTypes(StructType*, std::set<Type*>* type);
};



#endif /* HEADERS_DATALAYOUTHELPER_H_ */
