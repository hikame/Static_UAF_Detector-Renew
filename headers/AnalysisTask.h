#ifndef _ANALYSIS_TASK_H
#define _ANALYSIS_TASK_H

#include "AnalysisStructHeaders.h"

class AnalysisTask{
public:
	BasicBlock* basicBlock;
	std::shared_ptr<AnalysisState> analysisState;
	Instruction* startInst;
	UafDetectionPass* uafPass;
	ExecuteRelationship execRS;
	std::string parentThread;
	bool started = false;
	bool finished = false;
	AnalysisTask(BasicBlock* bb, std::shared_ptr<AnalysisState> as, Instruction* si, UafDetectionPass* up, ExecuteRelationship er, std::string pt):
		basicBlock(bb), analysisState(as), startInst(si), uafPass(up), execRS(er), parentThread(pt){}
};

#endif
