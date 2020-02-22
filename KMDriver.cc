//===-- KMDriver.cc - the KMDriver framework------------------------===//
// 
// This file implemets the KMDriver framework. It calls the pass for
// building call-graph and the pass for finding unsafe allocations.
// It outputs the information of unsafe allocations for further
// instrumentation (i.e., zero-initialization). The iterative 
// analysis strategy is borrowed from KINT[OSDI'12] to avoid 
// combining all bitcode files into a single one. 
//
//===-----------------------------------------------------------===//

#include "headers/KMDriver.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/SystemUtils.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/CommandLine.h"

#include <memory>
#include <vector>
#include <sstream>
#include <sys/resource.h>

#include "headers/CallGraph.h"
#include "headers/UafDetectionPass.h"
#include "headers/Config.h"
#include <iostream>
// #include <process.h>

using namespace llvm;

cl::opt<unsigned> VerboseLevel(
    "verbose-level", cl::desc("Print information at which verbose level"),
    cl::init(0));

cl::opt<unsigned> CPDepthLimit(
    "cplimit", cl::desc("The limit of call depth (default is 10)"),
    cl::init(10));

cl::opt<unsigned> BBAnalysisTimeLimit(
    "bbAnalyzeLimit", cl::desc("The limit of analysis times of basic blocks inside one function (default is 2)"),
    cl::init(2));   // set default to 2. for counters in a loop, 
                    // the first time of run will change the counters' value and make it symoblized

cl::opt<unsigned> FuncAnalysisTimeLimit(
    "funcAnalyzeLimit", cl::desc("The limit of analysis times of every function (default is 6)"),
    cl::init(6));

cl::opt<unsigned> ThreadsPerCPU(
    "tpcpu", cl::desc("The thread count per cpu (default is 1)"),
    cl::init(1));

cl::opt<bool> STUAF(
    "suaf",
    cl::desc("Detecting UAF in a single thread."), 
    cl::NotHidden, cl::init(false));

cl::opt<std::string> TGI(
    "tgi",
    cl::desc("Trust global initializer, defualt true (if false, the initializer of global function variable is still trusted)."),
    cl::NotHidden, cl::init(""));

cl::opt<std::string> CAAS(
    "ctn",
    cl::desc("Continue Analyze After Sunk."),
    cl::NotHidden, cl::init(""));

cl::opt<std::string> PRT_DB(
    "printDB",
    cl::desc("Print debug information (such the string of one instruction and creation of thread)."),
    cl::NotHidden, cl::init(""));

cl::opt<std::string> PRT_WARNING(
    "printWN",
    cl::desc("Print the warning information."),
    cl::NotHidden, cl::init(""));

cl::opt<std::string> PRT_TRACE(
    "printTC",
    cl::desc("Print the trace information of debug."),
    cl::NotHidden, cl::init(""));


cl::opt<std::string> PRT_ERROR(
    "printER",
    cl::desc("Print the error information."),
    cl::NotHidden, cl::init(""));


cl::opt<std::string> TargetMethod(
    "tm",
		cl::NormalFormatting,
		cl::desc("Target Method."),
		cl::init(""));

cl::opt<std::string> TargetMethodS1(
    "s1",
		cl::NormalFormatting,
		cl::desc("Target Method For Step 1."),
		cl::init(""));

cl::opt<std::string> TargetMethodS2(
    "s2",
		cl::NormalFormatting,
		cl::desc("Target Method For Step 2."),
		cl::init(""));

// Command line parameters.
cl::list<std::string> InputFilenames(
    cl::Positional, cl::OneOrMore, cl::desc("<input bitcode files>"));

cl::opt<std::string> OuputPath(
    "o",
		cl::NormalFormatting,
		cl::desc("Output target path (default is ./ResultOutput)."),
		cl::init("./ResultOutput"));

cl::opt<std::string> Ignored_Func_List_Record(
    "ifs",
		cl::NormalFormatting,
		cl::desc("Config file recording ignored fucntion list (default is ./ignored_function_list.conf)."),
		cl::init("./ignored_function_list.conf"));


GlobalContext GlobalCtx;
std::string StartMethod;

//KM: This is used to run the self-defined pass of LLVM
//KM: CG generation and Safe-Alloc analysis are all based on this method.
//KM: The internal variables in IterativeModulePass GlobalContext *Ctx & const char * ID can be used to seperate different passes.
void IterativeModulePass::run(ModuleList &modules) {
	std::string thdStr = GetThreadID();
  ModuleList::iterator i, e;
  OP << "[" << ID << "] Initializing " << modules.size() << " modules ";
  bool again = true;
  while (again) {
    again = false;
    for (i = modules.begin(), e = modules.end(); i != e; ++i) {
    again |= doInitialization(i->first);  //Initialize, CG is constructed here!
      OP << ".";
    }
  }
  OP << "\n";

  unsigned iter = 0, changed = 1;
  while (changed) {
    ++iter;
    changed = 0;
    for (i = modules.begin(), e = modules.end(); i != e; ++i) {
      OP << "[" << ID << " / " << iter << "] ";
      OP << "[" << i->second << "]\n";

      bool ret = doModulePass(i->first);  //Noting is done for CG generation here. 
      if (ret) {
        ++changed;
        OP << "\t [CHANGED]\n";
      } else
        OP << "\n";
    }
    OP << "[Tread-" << thdStr << "] " << " [" << ID << "] Updated in " << changed << " modules.\n";
  }

  OP << "[Tread-" << thdStr << "] " << " [" << ID << "] Postprocessing ...\n";
  again = true;
  while (again) {
    again = false;
    for (i = modules.begin(), e = modules.end(); i != e; ++i) {
    again |= doFinalization(i->first);  //KM: Nothing is done for CG generation here.
    }
  }

  OP << "[Tread-" << thdStr << "] " << " [" << ID << "] Done!\n\n";
}

void SetMallocFunctions(GlobalContext *GCtx) {
//  std::string HeapAllocFN[] = {
//    "__kmalloc",
//    "__vmalloc",
//    "vmalloc",
//    "krealloc",STUAF
//    "devm_kzalloc",
//    "vzalloc",
//    "malloc",
//    "kmem_cache_alloc",
//    "__alloc_skb",
//  };

  for (auto F : HeapAllocFN) {
  	GCtx->HeapAllocFuncs.insert(F);
  }
}

void SetLoopCheckSet(GlobalContext *GCtx){
	ExecuteRelationship list[] = {
			Branch_True,
			Branch_False,
			Branch_Undetermined,
			Branch_UnConditional,
			Switch_NotSure,
			Switch_Sure,
//			Select_True,
//			Select_False,
//			MallocResult_Failed,
//			MallocResult_Success,
//			CMP_True,
//			CMP_False,
//			Symbolic_Index
	};
	for(ExecuteRelationship er : list)
		GCtx->LoopCheckSet.insert(er);
}

void SetNoPrintSet(GlobalContext *GCtx){
	ExecuteRelationship list[] = {
		MallocResult_Failed ,
		MallocResult_Success ,
		Fake_Return_Null,
		Fake_Return_Nonnull,
		CMP_False ,
		CMP_True ,
		Select_True ,
		Select_False
	};
	for(ExecuteRelationship er : list)
		NoPrintSet.insert(er);
}

int main(int argc, char **argv)
{
  // Print a stack trace if we signal out.
  sys::PrintStackTraceOnErrorSignal(argv[0]);
  PrettyStackTraceProgram X(argc, argv);

  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.

  //KM: The above lines have prepared a debugable environment.

  //KM:cl::ParseCommandLineOptions is designed to be called directly from main, and is used to fill in the values of all of the command line option variables once argc and argv are available
  //KM: This function seems have nothing to do with the commond line parsing. It is likely to be used when some missing options need to be set to some default value. 
  cl::ParseCommandLineOptions(argc, argv, "global analysis\n");
  SMDiagnostic Err;

  // Loading modules
  // KM: The following codes also construct the GlobalCtx object.
  std::string thdStr = GetThreadID();
  OP << "[Tread-" << thdStr << "] " << " Total " << InputFilenames.size() << " file(s)\n";

  for (unsigned i = 0; i < InputFilenames.size(); ++i) {

    LLVMContext *LLVMCtx = new LLVMContext();
    std::unique_ptr<Module> M = parseIRFile(InputFilenames[i], Err, *LLVMCtx);

    if (M == NULL) {
      OP << argv[0] << ": error loading file '"
        << InputFilenames[i] << "'\n";
      continue;
    }
    //KM: load the IR file and parse it into a module object.
    Module *Module = M.release();
    StringRef MName = StringRef(strdup(InputFilenames[i].data()));
    GlobalCtx.Modules.push_back(std::make_pair(Module, MName));
    GlobalCtx.ModuleMaps[Module] = InputFilenames[i];
  }

  // Main workflow
  // Build global callgraph.
  CallGraphPass CGPass(&GlobalCtx);
  CGPass.run(GlobalCtx.Modules);

  // The safe allocation pass of KMDriver.
  if (STUAF) {
    //define the sink/source/init/heap alloc functions based on function names.
    SetMallocFunctions(&GlobalCtx);
    SetLoopCheckSet(&GlobalCtx);
    SetNoPrintSet(&GlobalCtx);
    StringRef targetMethodSR = TargetMethod.getValue();
    StartMethod = targetMethodSR.str();
    StringRef tmStep1 = TargetMethodS1.getValue();
    StringRef tmStep2 = TargetMethodS2.getValue();
    GlobalCtx.trustGlobalInit = (TGI.getValue() == "true");
    GlobalCtx.continueAAS = (CAAS.getValue() == "true");
    GlobalCtx.printDB = (PRT_DB.getValue() == "true");
    GlobalCtx.printWN = (PRT_WARNING.getValue() == "true");
    GlobalCtx.printTC = (PRT_TRACE.getValue() == "true");
    GlobalCtx.printER = (PRT_ERROR.getValue() == "true");
    GlobalCtx.cpLimit = CPDepthLimit;
    GlobalCtx.thdPerCPU = ThreadsPerCPU;
    GlobalCtx.bbAnaLimit = BBAnalysisTimeLimit;
    GlobalCtx.funcAnaLimit = FuncAnalysisTimeLimit;
    GlobalCtx.initIgnoredFuncs(Ignored_Func_List_Record.getValue());
    GlobalCtx.outputPath = OuputPath.getValue();

    OP << "[Tread-" << thdStr << "] @ [" << GetCurrentTime() << "] [INF] Target Method is: " << TargetMethod << "\n";
    // Changed by Kame Wang 20180903
    // OP << "[Tread-" << thdStr << "] " << " [INF] Target Method (S1) is: " << tmStep1 << "\n";
    // OP << "[Tread-" << thdStr << "] " << " [INF] Target Method (S2) is: " << tmStep2 << "\n";

    UafDetectionPass* UDPass = NULL;
    if(TargetMethod != "")
    	UDPass = new UafDetectionPass(targetMethodSR, true, &GlobalCtx);  //true for single thread.
    else if(tmStep1 != "" && tmStep2 != "")
    	UDPass = new UafDetectionPass(tmStep1, tmStep2, true, &GlobalCtx);  //true for single thread.

    if(UDPass != NULL){
      // start analysis of UAF
    	UDPass->run(GlobalCtx.Modules);
    }

    // Print results  
    // UDPass->PrintStatistics();
    delete UDPass;
  }
  // OP << "PRESS ANY KEY TO FINISH!\n";
  // getchar();
  return 0;
}

std::string GetERString(ExecuteRelationship er){
	std::string erstr;
	switch(er){
	case Start:
		erstr = "Start"; break;
	case Call:
		erstr = "Call"; break;
	case Return:
		erstr = "Return"; break;
	case Branch_True:
		erstr = "Branch_True"; break;
	case Branch_False:
		erstr = "Branch_False"; break;
	case Branch_Undetermined:
		erstr = "Branch_Undet"; break;
	case Branch_UnConditional:
		erstr = "Branch_UnCon"; break;
	case Switch_Sure:
		erstr = "Switch_Sure"; break;
	case Switch_NotSure:
			erstr = "Switch_NSure"; break;
	case Select_True:
		erstr = "Select_True"; break;
	case Select_False:
		erstr = "Select_False"; break;
	case MallocResult_Success:
		erstr = "Malloc_Success"; break;
	case MallocResult_Failed:
		erstr = "Malloc_Failed"; break;
	case Fake_Return_Nonnull:
		erstr = "Ret_MB"; break;
	case Fake_Return_Null:
		erstr = "Ret_Null"; break;
	case CMP_True:
		erstr = "CMP_True"; break;
	case CMP_False:
		erstr = "CMP_False"; break;
	case Symbolic_Index:
		erstr = "Symb_Index"; break;
	default:
		erstr = "Unknown";
	}
	return erstr;
}

std::string GetThreadID(){
	pthread_t pth = pthread_self();
	char buf[20];
	std::sprintf(buf, "%lx", pth);
	std::string ret(buf);
	return ret;
}

std::string GetCurrentTime(){
	 time_t nowtime;
	 nowtime = time(NULL); //获取日历时间
   std::string ret = ctime(&nowtime);
	 return ret.substr(0, ret.length() - 1);
}
