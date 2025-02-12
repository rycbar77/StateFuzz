//===- PTACallGraph.cpp -- Call graph used internally in SVF------------------//
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
 * PTACallGraph.cpp
 *
 *  Created on: Nov 7, 2013
 *      Author: Yulei Sui
 */

#include "Util/SVFModule.h"
#include "SVF-FE/LLVMUtil.h"
#include "Graphs/PTACallGraph.h"
#include <llvm/Support/CommandLine.h>

using namespace SVFUtil;


static llvm::cl::opt<bool> CallGraphDotGraph("dump-callgraph", llvm::cl::init(false),
                                       llvm::cl::desc("Dump dot graph of Call Graph"));

PTACallGraph::CallSiteToIdMap PTACallGraph::csToIdMap;
PTACallGraph::IdToCallSiteMap PTACallGraph::idToCSMap;
CallSiteID PTACallGraph::totalCallSiteNum = 1;


/// Add direct and indirect callsite
//@{
void PTACallGraphEdge::addDirectCallSite(const CallBlockNode* call) {
    assert(SVFUtil::getCallee(call->getCallSite()) && "not a direct callsite??");
    directCalls.insert(call);
}

void PTACallGraphEdge::addInDirectCallSite(const CallBlockNode* call) {
    assert((NULL == SVFUtil::getCallee(call->getCallSite()) || NULL == SVFUtil::dyn_cast<Function> (SVFUtil::getForkedFun(call->getCallSite()))) && "not an indirect callsite??");
    indirectCalls.insert(call);
}
//@}

bool PTACallGraphNode::isReachableFromProgEntry() const
{
    std::stack<const PTACallGraphNode*> nodeStack;
    NodeBS visitedNodes;
    nodeStack.push(this);
    visitedNodes.set(getId());

    while (nodeStack.empty() == false) {
        PTACallGraphNode* node = const_cast<PTACallGraphNode*>(nodeStack.top());
        nodeStack.pop();

        if (SVFUtil::isProgEntryFunction(node->getFunction()))
            return true;

        for (const_iterator it = node->InEdgeBegin(), eit = node->InEdgeEnd(); it != eit; ++it) {
            PTACallGraphEdge* edge = *it;
            if (visitedNodes.test_and_set(edge->getSrcID()))
                nodeStack.push(edge->getSrcNode());
        }
    }

    return false;
}


/// Constructor
PTACallGraph::PTACallGraph(CGEK k): kind(k) {
    callGraphNodeNum = 0;
    numOfResolvedIndCallEdge = 0;
}

/*!
 *  Memory has been cleaned up at GenericGraph
 */
void PTACallGraph::destroy() {
}

/*!
 * Add call graph node
 */
void PTACallGraph::addCallGraphNode(const Function* fun) {
    NodeID id = callGraphNodeNum;
    PTACallGraphNode* callGraphNode = new PTACallGraphNode(id, fun);
    addGNode(id,callGraphNode);
    funToCallGraphNodeMap[fun] = callGraphNode;
    callGraphNodeNum++;
}

/*!
 *  Whether we have already created this call graph edge
 */
PTACallGraphEdge* PTACallGraph::hasGraphEdge(PTACallGraphNode* src, PTACallGraphNode* dst,PTACallGraphEdge::CEDGEK kind, CallSiteID csId) const {
    PTACallGraphEdge edge(src,dst,kind,csId);
    PTACallGraphEdge* outEdge = src->hasOutgoingEdge(&edge);
    PTACallGraphEdge* inEdge = dst->hasIncomingEdge(&edge);
    if (outEdge && inEdge) {
        assert(outEdge == inEdge && "edges not match");
        return outEdge;
    }
    else
        return NULL;
}

/*!
 * get CallGraph edge via nodes
 */
PTACallGraphEdge* PTACallGraph::getGraphEdge(PTACallGraphNode* src, PTACallGraphNode* dst,PTACallGraphEdge::CEDGEK kind, CallSiteID csId) {
    for (PTACallGraphEdge::CallGraphEdgeSet::iterator iter = src->OutEdgeBegin();
            iter != src->OutEdgeEnd(); ++iter) {
        PTACallGraphEdge* edge = (*iter);
        if (edge->getEdgeKind() == kind && edge->getDstID() == dst->getId())
            return edge;
    }
    return NULL;
}

/*!
 * Add direct call edges
 */
void PTACallGraph::addDirectCallGraphEdge(const CallBlockNode* cs,const Function* callerFun, const Function* calleeFun) {

    PTACallGraphNode* caller = getCallGraphNode(callerFun);
    PTACallGraphNode* callee = getCallGraphNode(calleeFun);

    CallSiteID csId = addCallSite(cs, callee->getFunction());

    if(!hasGraphEdge(caller,callee, PTACallGraphEdge::CallRetEdge,csId)) {
        PTACallGraphEdge* edge = new PTACallGraphEdge(caller,callee,PTACallGraphEdge::CallRetEdge,csId);
        edge->addDirectCallSite(cs);
        addEdge(edge);
        callinstToCallGraphEdgesMap[cs].insert(edge);
    }
}

/*!
 * Add indirect call edge to update call graph
 */
void PTACallGraph::addIndirectCallGraphEdge(const CallBlockNode* cs,const Function* callerFun, const Function* calleeFun) {

	PTACallGraphNode* caller = getCallGraphNode(callerFun);
    PTACallGraphNode* callee = getCallGraphNode(calleeFun);

    numOfResolvedIndCallEdge++;

    CallSiteID csId = addCallSite(cs, callee->getFunction());

    if(!hasGraphEdge(caller,callee, PTACallGraphEdge::CallRetEdge,csId)) {
        PTACallGraphEdge* edge = new PTACallGraphEdge(caller,callee,PTACallGraphEdge::CallRetEdge, csId);
        edge->addInDirectCallSite(cs);
        addEdge(edge);
        callinstToCallGraphEdgesMap[cs].insert(edge);
    }
}

/*!
 * Get all callsite invoking this callee
 */
void PTACallGraph::getAllCallSitesInvokingCallee(const Function* callee, PTACallGraphEdge::CallInstSet& csSet) {
    PTACallGraphNode* callGraphNode = getCallGraphNode(callee);
    for(PTACallGraphNode::iterator it = callGraphNode->InEdgeBegin(), eit = callGraphNode->InEdgeEnd();
            it!=eit; ++it) {
        for(PTACallGraphEdge::CallInstSet::iterator cit = (*it)->directCallsBegin(),
                ecit = (*it)->directCallsEnd(); cit!=ecit; ++cit) {
            csSet.insert((*cit));
        }
        for(PTACallGraphEdge::CallInstSet::iterator cit = (*it)->indirectCallsBegin(),
                ecit = (*it)->indirectCallsEnd(); cit!=ecit; ++cit) {
            csSet.insert((*cit));
        }
    }
}

/*!
 * Get direct callsite invoking this callee
 */
void PTACallGraph::getDirCallSitesInvokingCallee(const Function* callee, PTACallGraphEdge::CallInstSet& csSet) {
    PTACallGraphNode* callGraphNode = getCallGraphNode(callee);
    for(PTACallGraphNode::iterator it = callGraphNode->InEdgeBegin(), eit = callGraphNode->InEdgeEnd();
            it!=eit; ++it) {
        for(PTACallGraphEdge::CallInstSet::iterator cit = (*it)->directCallsBegin(),
                ecit = (*it)->directCallsEnd(); cit!=ecit; ++cit) {
            csSet.insert((*cit));
        }
    }
}

/*!
 * Get indirect callsite invoking this callee
 */
void PTACallGraph::getIndCallSitesInvokingCallee(const Function* callee, PTACallGraphEdge::CallInstSet& csSet) {
    PTACallGraphNode* callGraphNode = getCallGraphNode(callee);
    for(PTACallGraphNode::iterator it = callGraphNode->InEdgeBegin(), eit = callGraphNode->InEdgeEnd();
            it!=eit; ++it) {
        for(PTACallGraphEdge::CallInstSet::iterator cit = (*it)->indirectCallsBegin(),
                ecit = (*it)->indirectCallsEnd(); cit!=ecit; ++cit) {
            csSet.insert((*cit));
        }
    }
}

/*!
 * Issue a warning if the function which has indirect call sites can not be reached from program entry.
 */
void PTACallGraph::verifyCallGraph()
{
    CallEdgeMap::const_iterator it = indirectCallMap.begin();
    CallEdgeMap::const_iterator eit = indirectCallMap.end();
    for (; it != eit; ++it) {
        const FunctionSet& targets = it->second;
        if (targets.empty() == false) {
            const CallBlockNode* cs = it->first;
            const Function* func = cs->getCaller();
            if (getCallGraphNode(func)->isReachableFromProgEntry() == false)
                wrnMsg(func->getName().str() + " has indirect call site but not reachable from main");
        }
    }
}

/*!
 * Dump call graph into dot file
 */
void PTACallGraph::dump(const std::string& filename) {
    if(CallGraphDotGraph)
        GraphPrinter::WriteGraphToFile(outs(), filename, this);

}


namespace llvm {

/*!
 * Write value flow graph into dot file for debugging
 */
template<>
struct DOTGraphTraits<PTACallGraph*> : public DefaultDOTGraphTraits {

    typedef PTACallGraphNode NodeType;
    typedef NodeType::iterator ChildIteratorType;
    DOTGraphTraits(bool isSimple = false) :
        DefaultDOTGraphTraits(isSimple) {
    }

    /// Return name of the graph
    static std::string getGraphName(PTACallGraph *graph) {
        return "Call Graph";
    }
    /// Return function name;
    static std::string getNodeLabel(PTACallGraphNode *node, PTACallGraph *graph) {
        return node->getFunction()->getName().str();
    }

    static std::string getNodeAttributes(PTACallGraphNode *node, PTACallGraph *PTACallGraph) {
        const Function* fun = node->getFunction();
        if (!SVFUtil::isExtCall(fun)) {
            return "shape=circle";
        } else
            return "shape=Mrecord";
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(PTACallGraphNode *node, EdgeIter EI, PTACallGraph *PTACallGraph) {

        //TODO: mark indirect call of Fork with different color
        PTACallGraphEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string color;

        if (edge->getEdgeKind() == PTACallGraphEdge::TDJoinEdge) {
            color = "color=green";
        } else if (edge->getEdgeKind() == PTACallGraphEdge::TDForkEdge) {
            color = "color=blue";
        } else {
            color = "color=black";
        }
        if (0 != edge->getIndirectCalls().size()) {
            color = "color=red";
        }
        return color;
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType *node, EdgeIter EI) {
    	PTACallGraphEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string str;
        raw_string_ostream rawstr(str);
        rawstr << edge->getCallSiteID();

        return rawstr.str();
    }
};
}
