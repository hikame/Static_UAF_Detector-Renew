/*
 * MethodWrapper.h
 *
 *  Created on: 2017年5月27日
 *      Author: kame
 */

#ifndef HEADERS_FUNCTIONWRAPPER_H_
#define HEADERS_FUNCTIONWRAPPER_H_

#include "AnalysisStructHeaders.h"
#include "KMDriver.h"

class FunctionWrapper{
public:
	enum HandleResult{
		HasWrapper,
		NoWrapper,
		NeedAbort
	};
	FunctionWrapper(GlobalContext *gc){
		globalContext = gc;
	};

	FunctionWrapper::HandleResult HandleFunction(Function*, CallInst*, std::shared_ptr<AnalysisState>);
	static bool CheckPrefix(std::string, std::string);
	// static bool HasWrapper(std::string);
private:
	void HandleMemcpy(Value* sourceValue, Value* destValue, uint64_t size, std::shared_ptr<AnalysisState> as);
	void HandleMemset(CallInst*, std::shared_ptr<AnalysisState>, bool);
	void HandleMemmove(Value* sourceValue, Value* destValue, uint64_t size, std::shared_ptr<AnalysisState> as);
	size_t GetBasicFieldsInRange(std::shared_ptr<MemoryBlock> mb, 
		uint64_t startpos, uint64_t size, std::vector<std::shared_ptr<MemoryBlock>>& record, std::shared_ptr<AnalysisState> as);
	size_t GetBasicFieldsInRange(MemoryBlock* mb, uint64_t startpos, 
		uint64_t size, std::vector<std::shared_ptr<MemoryBlock>>& record, std::shared_ptr<AnalysisState> as);
	bool IsBasicField(MemoryBlock* mb);

	GlobalContext* globalContext;
};




#endif /* HEADERS_FUNCTIONWRAPPER_H_ */
