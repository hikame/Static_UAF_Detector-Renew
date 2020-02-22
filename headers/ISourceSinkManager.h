#ifndef _ISS_MANAGER_H
#define _ISS_MANAGER_H

#include "AnalysisStructHeaders.h"

//using namespace llvm;

class ISourceSinkManager{  
 public:
  virtual bool IsSource(Instruction* i) = 0;
//  virtual bool IsSink(Instruction* inst, Value* taintedValue, TrackState* trackState) = 0;
  virtual bool IsSink(Instruction* inst, std::shared_ptr<AnalysisState> analysisState) = 0;
  virtual ~ISourceSinkManager(){};
};

#endif
