/*
 * StorageElementType.h
 *
 *  Created on: 2020年2月13日
 *      Author: kame
 */

#ifndef STOREDELEMENT_H_
#define STOREDELEMENT_H_

#include "AnalysisStructHeaders.h"
#include "KMDriver.h"


/** PARENT class of all storage element types, 
 *  currently incluing SymbolicValue, MemoryBlock and
 *  ConstantValueWrapper
 */
class StoredElement{
public:
    virtual ~StoredElement(){

    };
};

#endif /* STOREDELEMENT_H_ */
