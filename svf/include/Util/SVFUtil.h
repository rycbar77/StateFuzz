//===- SVFUtil.h -- Analysis helper functions----------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * SVFUtil.h
 *
 *  Created on: Apr 11, 2013
 *      Author: Yulei Sui, dye
 */

#ifndef AnalysisUtil_H_
#define AnalysisUtil_H_

#include "SVF-FE/LLVMModule.h"
#include "Util/BasicTypes.h"
#include <time.h>

/*
 * Util class to assist pointer analysis
 */
namespace SVFUtil {

/// Overwrite llvm::outs()
inline raw_ostream &outs(){
	return llvm::outs();
}

/// Overwrite llvm::errs()
inline raw_ostream &errs(){
	return llvm::errs();
}

/// Dump sparse bitvector set
void dumpSet(NodeBS To, raw_ostream & O = SVFUtil::outs());

/// Dump points-to set
void dumpPointsToSet(unsigned node, NodeBS To) ;

/// Dump alias set
void dumpAliasSet(unsigned node, NodeBS To) ;

/// Print successful message by converting a string into green string output
std::string sucMsg(std::string msg);

/// Print warning message by converting a string into yellow string output
void wrnMsg(std::string msg);

/// Print error message by converting a string into red string output
//@{
std::string  errMsg(std::string msg);
std::string  bugMsg1(std::string msg);
std::string  bugMsg2(std::string msg);
std::string  bugMsg3(std::string msg);
//@}

/// Print each pass/phase message by converting a string into blue string output
std::string  pasMsg(std::string msg);

/// Print memory usage in KB.
void reportMemoryUsageKB(const std::string& infor, raw_ostream & O = SVFUtil::outs());

/// Get memory usage from system file. Return TRUE if succeed.
bool getMemoryUsageKB(u32_t* vmrss_kb, u32_t* vmsize_kb);

/// Increase the stack size limit
void increaseStackSize();

/*!
 * Compare two PointsTo according to their size and points-to elements.
 * 1. PointsTo with smaller size is smaller than the other;
 * 2. If the sizes are equal, comparing the points-to targets.
 */
inline bool cmpPts (const PointsTo& lpts,const PointsTo& rpts) {
    if (lpts.count() != rpts.count())
        return (lpts.count() < rpts.count());
    else {
        PointsTo::iterator bit = lpts.begin(), eit = lpts.end();
        PointsTo::iterator rbit = rpts.begin(), reit = rpts.end();
        for (; bit != eit && rbit != reit; bit++, rbit++) {
            if (*bit != *rbit)
                return (*bit < *rbit);
        }

        return false;
    }
}

/// Return true if this function is llvm dbg intrinsic function/instruction
//@{
inline bool isIntrinsicDbgFun(const Function* fun) {
    return fun->getName().startswith("llvm.dbg.declare") ||
           fun->getName().startswith("llvm.dbg.value");
}
/// Return true if it is an intric debug instruction
inline bool isInstrinsicDbgInst(const Instruction* inst) {
    return SVFUtil::isa<llvm::DbgInfoIntrinsic>(inst);
}
//@}

inline bool isConstantData(const Value* val) {
	return SVFUtil::isa<ConstantData>(val)
			|| SVFUtil::isa<ConstantAggregate>(val)
			|| SVFUtil::isa<MetadataAsValue>(val)
			|| SVFUtil::isa<BlockAddress>(val);
}

/// Whether an instruction is a call or invoke instruction
inline bool isCallSite(const Instruction* inst) {
    return SVFUtil::isa<CallInst>(inst) || SVFUtil::isa<InvokeInst>(inst);
}
/// Whether an instruction is a callsite in the application code, excluding llvm intrinsic calls
inline bool isNonInstricCallSite(const Instruction* inst) {
	if(isInstrinsicDbgInst(inst))
		return false;
    return isCallSite(inst);
}
/// Whether an instruction is a return instruction
inline bool isReturn(const Instruction* inst) {
    return SVFUtil::isa<ReturnInst>(inst);
}

/// Return LLVM callsite given a instruction
inline CallSite getLLVMCallSite(const Instruction* inst) {
    assert(SVFUtil::isa<CallInst>(inst)|| SVFUtil::isa<InvokeInst>(inst));
    CallSite cs(const_cast<Instruction*>(inst));
    return cs;
}

/// Get the corresponding Function based on its name
inline Function* getFunction(StringRef name) {
    Function* fun = NULL;
    for (u32_t i = 0; i < LLVMModuleSet::getLLVMModuleSet()->getModuleNum(); ++i) {
        Module *mod = LLVMModuleSet::getLLVMModuleSet()->getModule(i);
        fun = mod->getFunction(name);
        if(fun && !fun->isDeclaration()) {
            return fun;
        }
    }
    return fun;
}

/// Get the definition of a function across multiple modules
inline const Function* getDefFunForMultipleModule(const Function* fun) {
	if(fun == NULL) return NULL;

    if (fun->isDeclaration() && LLVMModuleSet::getLLVMModuleSet()->hasDefinition(fun))
        fun = LLVMModuleSet::getLLVMModuleSet()->getDefinition(fun);
    return fun;
}

/// Return callee of a callsite. Return null if this is an indirect call
//@{
inline const Function* getCallee(const CallSite cs) {
    // FIXME: do we need to strip-off the casts here to discover more library functions
    Function *callee = SVFUtil::dyn_cast<Function>(cs.getCalledValue()->stripPointerCasts());
    return getDefFunForMultipleModule(callee);
}

inline const Function* getCallee(const Instruction *inst) {
    if (!SVFUtil::isa<CallInst>(inst) && !SVFUtil::isa<InvokeInst>(inst))
        return NULL;
    CallSite cs(const_cast<Instruction*>(inst));
    return getCallee(cs);
}
//@}

/// Return source code including line number and file name from debug information
//@{
std::string  getSourceLoc(const Value *val);
std::string  getSourceLocOfFunction(const Function *F);
//@}

}

#endif /* AnalysisUtil_H_ */
