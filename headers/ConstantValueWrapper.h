/*
 * MemoryBlock.h
 *
 *  Created on: 2017年5月18日
 *      Author: kame
 */

#ifndef HEADERS_ConstantValueWrapper_H_
#define HEADERS_ConstantValueWrapper_H_

#include "AnalysisStructHeaders.h"
#include "KMDriver.h"

class ConstantValueWrapper : public StoredElement{
public:
	Value* containedValue;
	ConstantValueWrapper(Value* v);
};
#endif /* HEADERS_ConstantValueWrapper_H_ */
