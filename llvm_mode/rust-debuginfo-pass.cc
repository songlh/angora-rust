

#include "llvm/IR/Module.h"
#include "llvm/IR/DebugInfoMetadata.h"

using namespace llvm;

namespace {
	class RustDebugInfo : public FunctionPass {
	public:
		static char ID;
		RustDebugInfo();
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual bool runOnFunction(Function &F);
	};
}

static RegisterPass<RustDebugInfo> X("print-Rust-DebugInfo", "print debuginfo for Rust", false, false);

char RustDebugInfo::ID = 0;

void RustDebugInfo::getAnalysisUsage(AnalysisUsage &AU) const {

}

RustDebugInfo::RustDebugInfo() : FunctionPass(ID) {}

bool RustDebugInfo::runOnFunction(Function & F) 
{

	if(F.getName().find("test_main") == StringRef::npos)
	{
		return false;
	}

	errs() << F.getName()  << "\n";


	for(Function::iterator BB = F.begin(); BB != F.end(); BB ++ )
	{
		for(BasicBlock::iterator II = BB->begin(); II != BB->end(); II ++)
		{
			Instruction * pI = &* II;

			if(DILocation * Loc = pI->getDebugLoc())
			{
				errs() << Loc->getDirectory() << "/" << Loc->getFilename() << ": " << Loc->getLine() << "\n";
			}
		}
	}

	return false;
}

