#ifndef _UAF_SSM_H
#define _UAF_SSM_H

#include "ISourceSinkManager.h"
#include "AnalysisStructHeaders.h"
#include "KMDriver.h"

//using namespace llvm;
struct ExecutionRecord;
struct CallRecord;

class UAFSourceSinkManager final : public ISourceSinkManager{
private:
  GlobalContext* globalContext;
  std::set<SinkRecord*> sinkRecords;

public:
  //@Override
  bool IsSource(Instruction* i);
  bool IsSink(Instruction* inst, std::shared_ptr<AnalysisState> analysisState);
  // -1 for terminate, 0 for should not analyze the inst, 1 for should continue analyze the inst
  int AnalyzeTag(Instruction* inst, std::shared_ptr<AnalysisState> analysisState);
  SinkRecord* RecordSinkPath(std::shared_ptr<MemoryBlock> mb, Instruction* inst, std::shared_ptr<AnalysisState> analysisState);
  void CheckHeapUAF(std::shared_ptr<AnalysisState>, MemoryBlock*, CallRecord* cr = NULL);
  UAFSourceSinkManager(GlobalContext *gc);

  ~UAFSourceSinkManager(){};

private:
  unsigned getArgNo(Value* tv, CallInst* ci);
};

#endif
