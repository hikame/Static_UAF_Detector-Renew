/*
 * AnalysisStructHeaders.h
 *
 *  Created on: 2017年5月18日
 *      Author: kame
 */

#ifndef HEADERS_ANALYSISSTRUCTHEADERS_H_
#define HEADERS_ANALYSISSTRUCTHEADERS_H_

#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Analysis/BasicAliasAnalysis.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/PassAnalysisSupport.h>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
using namespace llvm;
class StoredElement;
class AnalysisState;
class AnalysisTask;
class FieldRelationship;
class MemoryBlock;
class UAFSourceSinkManager;
class UafDetectionPass;
class FunctionWrapper;
class DataLayoutHelper;
class SymbolicValue;
class LocalVar;
class ConstantValueWrapper;
class SinkRecord;

#include "Common.h"
#include "KMDriver.h"
#include "SinkRecord.h"
#include "AnalysisState.h"
#include "AnalysisTask.h"
#include "StoredElement.h"
#include "FieldRelationship.h"
#include "MemoryBlock.h"
#include "UAFSourceSinkManager.h"
#include "UafDetectionPass.h"
#include "FunctionWrapper.h"
#include "DataLayoutHelper.h"
#include "SymbolicValue.h"
#include "LocalVar.h"
#include "ConstantValueWrapper.h"
#endif /* HEADERS_ANALYSISSTRUCTHEADERS_H_ */
