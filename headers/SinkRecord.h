#ifndef _UAF_SinkRecord_H123
#define _UAF_SinkRecord_H123

#include "AnalysisStructHeaders.h"
#include <fstream>

struct ExecutionRecord;
struct CallRecord;
extern std::string StartMethod;

class SinkRecord{
public:
	GlobalContext* globalContext = NULL;
	std::shared_ptr<MemoryBlock> memoryBlock = NULL;
	Instruction* sinkInst = NULL;
	Instruction* freeInst = NULL;
	Instruction* sourceInst = NULL;

	std::mutex opLock;
	std::set<std::string> pathRecords;
	std::ofstream opStream;

	std::set<Function*> printedFunc;
	std::mutex pfLock;
	std::set<BasicBlock*> printedBB;
	std::mutex bbLock;

	SinkRecord(std::shared_ptr<MemoryBlock> mb, Instruction* sinkInst, Instruction* freeInst, GlobalContext* ctx);

	~SinkRecord();
	void HandleNewPath(std::list<std::shared_ptr<ExecutionRecord>>& execRecords, bool firstRecord = false);
	void RecordPathIntoFile(std::string record);
	void RecordStringIntoFile(std::string record);
	void RecordBasicBlock(BasicBlock* bb, bool important);
	void RecordFunction(Function* func, bool important);

};
#endif
