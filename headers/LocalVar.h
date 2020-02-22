/*
 * MemoryBlock.h
 *
 *  Created on: 2017年5月18日
 *      Author: kame
 */

#ifndef HEADERS_HeapVariable_H_
#define HEADERS_HeapVariable_H_

#include "AnalysisStructHeaders.h"

#define HeapVariableSubclassID 235

class LocalVar : public Value{
public:
	CallRecord* callRecord;
	Value* variable;
	LocalVar(Type * tp, CallRecord* cr, Value* var):Value(tp, HeapVariableSubclassID), callRecord(cr), variable(var){}
	static bool classof(const Value *V){
	 return V->getValueID() == HeapVariableSubclassID;
	}
};



#endif /* HEADERS_HeapVariable_H_ */
