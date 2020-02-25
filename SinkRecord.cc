#include "headers/SinkRecord.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/Value.h>

std::set<ExecuteRelationship> NoPrintSet;
//GVC_t* rgContext;
std::mutex rgLock;

SinkRecord::SinkRecord(std::shared_ptr<MemoryBlock> mb, Instruction* si, 
		Instruction* fi, GlobalContext* ctx, std::shared_ptr<AnalysisState> as){
	globalContext = ctx;
	analysisState = as;
	memoryBlock = mb;
	sinkInst = si;
	freeInst = fi;
	if(Value* baseValue = memoryBlock->GetBaseValue())
		if(Instruction* inst = dyn_cast<Instruction>(baseValue))
			sourceInst = inst;

	std::string fileName;
	std::stringstream tmpStream;
	int count = 0;
	while(true){
		tmpStream << globalContext->outputPath << "/" << StartMethod << ":" << freeInst->getParent()
			<< "(" <<  freeInst->getParent()->getParent()->getName().str() << ")"
			<< "&" << sinkInst->getParent()
			<< "(" <<  sinkInst->getParent()->getParent()->getName().str() << ")" << count
			<< ".kmd";
		fileName = tmpStream.str();
		std::ifstream fin(fileName);
		if (!fin)
			break;
		count++;
		tmpStream.str("");
	}
	fileName = tmpStream.str();
	opStream.open(fileName, std::ios::out|std::ios::ate);
	opStream << "H:";
	if(sourceInst != NULL){
		opStream << sourceInst->getParent()
				<< "(" <<  sourceInst->getParent()->getParent()->getName().str() << ")";
	}
	opStream << "|";
	opStream << freeInst->getParent()
				<< "(" <<  freeInst->getParent()->getParent()->getName().str() << ")|";
	opStream << sinkInst->getParent()
				<< "(" <<  sinkInst->getParent()->getParent()->getName().str() << ")\n";

	opStream << "Malloc Inst: " << globalContext->GetInstStr(sourceInst) << "\n";
	opStream << "Free Inst: " << globalContext->GetInstStr(freeInst) << "\n";
	opStream << "Reuse Inst: " << globalContext->GetInstStr(sinkInst) << "\n";
	opStream << as->GetWarningTypes() << "\n";
	opStream.flush();
	return;
}

SinkRecord::~SinkRecord(){
	opStream.flush();
	opStream.close();
}

void SinkRecord::RecordBasicBlock(BasicBlock* bb, bool important){
	std::lock_guard<std::mutex> lg(bbLock);
	if(printedBB.count(bb) > 0)
		return;
	printedBB.insert(bb);
	std::set<int> lineNums;
	BasicBlock::InstListType& instList = bb->getInstList();
	for(BasicBlock::iterator instIT = instList.begin(); instIT != instList.end(); instIT++){
		Instruction* inst = &*instIT;
		if(!inst->hasMetadata())
			continue;

		DebugLoc dl = inst->getDebugLoc();
		if(dl.get() == NULL)
			continue;
		lineNums.insert(dl->getLine());
	}

	std::stringstream tmpSS;
	char imflag = 'U';
	if(important)
		imflag = 'I';
	tmpSS << "B(" << imflag << "):";
	if(lineNums.size() > 0){
		tmpSS << bb << "(" << bb->getParent()->getName().str() << ")=";
		std::set<int>::iterator it = lineNums.begin();
		int startNum = *it;
		int endNum = *it;
		it++;
		std::set<int>::iterator end = lineNums.end();
		while(it != end){
			if(*it == endNum + 1)
				endNum++;
			else{
				tmpSS << startNum;
				if(startNum != endNum)
					tmpSS << "-" << endNum;
				tmpSS << ",";
				startNum = endNum = *it;
			}
			it++;
		}
		tmpSS << startNum;
		if(startNum != endNum)
			tmpSS << "-" << endNum;
	}
	else
		tmpSS << bb << "(" << bb->getParent()->getName().str() << ")=?";

	// for Warn Record
	tmpSS << "|";
	std::set<WarningType> wrnSet;
	analysisState->GetWarnRecord(bb, wrnSet);
	if(size_t size = wrnSet.size()){
		tmpSS << "W";
		size_t i = 0;
		for(auto w : wrnSet){
			tmpSS << w;
			if(i < size - 1)
				tmpSS << ", ";
			i++;
		}
	}

	tmpSS << "\n";
	RecordStringIntoFile(tmpSS.str());
	opStream.flush();
}

void SinkRecord::RecordFunction(Function* func, bool important){
	std::lock_guard<std::mutex> lg(pfLock);
	if(printedFunc.count(func) > 0)
		return;
	printedFunc.insert(func);
	MDNode *funcMDN = func->getMetadata("dbg");
	DISubprogram* dis = dyn_cast<DISubprogram>(funcMDN);
	std::stringstream tmpSS;
	char imflag = 'U';
	if(important)
		imflag = 'I';
	tmpSS << "F(" << imflag << "):" << func->getName().str() << "=";
	std::string director = dis->getDirectory();
	// std::string::size_type leftpos = director.find("msm");
	// if(leftpos == std::string::npos)
	// 	leftpos = 0;
	// std::string::size_type lenth = director.length();
	// std::string director = director.substr(leftpos, lenth - leftpos);
	tmpSS << director << "/" << dis->getFilename().str() << "\n";
	RecordStringIntoFile(tmpSS.str());
	opStream.flush();
}

void SinkRecord::HandleNewPath(std::list<std::shared_ptr<ExecutionRecord>>& executionPath, bool firstRecord){
	std::stringstream edgeStream;
	std::string preNode;
	int count = 0;
	// try to figure out some unimportant functions
	std::set<Function*> inFuncs;   // unimportant functions
	std::set<Function*> callstack;   // unimportant functions
	BasicBlock* srcBB = NULL;
	BasicBlock* freeBB = NULL;
	BasicBlock* sinkBB = NULL;
	if(sourceInst != NULL)
		srcBB = sourceInst->getParent();
	if(freeInst != NULL)
		freeBB = freeInst->getParent();
	if(sinkInst != NULL)
		sinkBB = sinkInst->getParent();
	
	BasicBlock* lastbb = NULL;
	for(std::list<std::shared_ptr<ExecutionRecord>>::iterator it = executionPath.begin();
			it != executionPath.end(); it++){
		std::shared_ptr<ExecutionRecord> ercd = *it;
		ExecuteRelationship currentER = ercd->er;
		if(currentER == Start || currentER == Call)
			callstack.insert(ercd->bb->getParent());
		else if(currentER == Return){
			Function* retfrom = lastbb->getParent();
			callstack.erase(retfrom);
		}
		if(ercd->bb == srcBB || ercd->bb == freeBB || ercd->bb == sinkBB)
			inFuncs.insert(callstack.begin(), callstack.end());
		lastbb = ercd->bb;
	}

	bool preNodeImportant = true;
	size_t nmEdgeCount = 0;  // normal mode edge count
	size_t smEdgeCount = 0;  // simplified mode edge count
	for(std::list<std::shared_ptr<ExecutionRecord>>::iterator it = executionPath.begin();
			it != executionPath.end();
			it++){

		std::shared_ptr<ExecutionRecord> ercd = *it;
		ExecuteRelationship currentER = ercd->er;
		std::string erStr = GetERString(currentER);
		edgeStream << erStr << "&"; //record the execution relathionship string
		
		if(NoPrintSet.count(currentER) == 0){ // we need to handle this node!
			BasicBlock* currentBB = ercd->bb;
			Function* currentFunc = currentBB->getParent();
			bool funcImportant = inFuncs.find(currentFunc) != inFuncs.end();
			RecordBasicBlock(currentBB, funcImportant);
			RecordFunction(currentFunc, funcImportant);
			std::stringstream cnStream;
			cnStream << currentBB << "(" << currentFunc->getName().str() << ")";
			std::string currentNode = cnStream.str();
			if(it != executionPath.begin()){
				count++;
				std::stringstream recordStream;
				char imflag = 'U';
				bool isImportant = (preNodeImportant && funcImportant);
				if(isImportant)
					imflag = 'I';
				recordStream << "E(" << imflag << "):" << preNode << "|" << edgeStream.str() << "|" << currentNode << "|";
				if(firstRecord)
					recordStream << nmEdgeCount << "|" << smEdgeCount;
				RecordPathIntoFile(recordStream.str());
				nmEdgeCount++;
				if(isImportant)
					smEdgeCount++;
				edgeStream.str("");
			}
			preNode = currentNode;
			preNodeImportant = funcImportant;
		}
	}
	if(count == 0){
		std::stringstream recordStream;
		recordStream << "E:" << preNode << "||";
		RecordPathIntoFile(recordStream.str());
	}
	opStream.flush();
}

void SinkRecord::RecordPathIntoFile(std::string record){
	std::lock_guard<std::mutex> lg(opLock);
	if(pathRecords.count(record))
		return;
	pathRecords.insert(record);
	opStream << record << "\n";
}

void SinkRecord::RecordStringIntoFile(std::string record){
	std::lock_guard<std::mutex> lg(opLock);
	opStream << record;
}
