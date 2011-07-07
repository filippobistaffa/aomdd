/*
 *  NodeManager.cpp
 *  aomdd
 *
 *  Created by William Lam on May 13, 2011
 *  Copyright 2011 UC Irvine. All rights reserved.
 *
 */

#include "MetaNode.h"
#include "NodeManager.h"
#include "utils.h"

namespace aomdd {
using namespace std;

// Be sure to test to see if sets give the same hash if items are inserted
// in different orders
size_t hash_value(const Operation &o) {
    size_t seed = 0;
    boost::hash_combine(seed, o.GetOperator());
    BOOST_FOREACH(ParamSet::value_type i, o.GetParamSet()) {
        boost::hash_combine(seed, i.get());
    }
    return seed;

}
bool operator==(const Operation &lhs, const Operation &rhs) {
    return lhs.GetOperator() == rhs.GetOperator() && lhs.GetParamSet()
            == rhs.GetParamSet();
}

// =================

NodeManager *NodeManager::GetNodeManager() {
    if (!initialized) {
        singleton = new NodeManager();
        initialized = true;
        return singleton;
    }
    else {
        return singleton;
    }
}

vector<MetaNodePtr> NodeManager::ReweighNodes(const vector<MetaNodePtr> &nodes, double w) {
    vector<MetaNodePtr> ret;
    BOOST_FOREACH(MetaNodePtr i, nodes) {
        if (i.get() != MetaNode::GetZero().get() &&
                i.get() != MetaNode::GetOne().get()) {
            MetaNodePtr newNode(new MetaNode(*i));
            newNode->SetWeight(newNode->GetWeight() * w);
            ret.push_back(newNode);
        }
        else {
            ret.push_back(i);
        }
    }
    return ret;
}

MetaNodePtr NodeManager::CreateMetaNode(const Scope &var,
        const vector<ANDNodePtr> &ch, double weight) {
    MetaNodePtr temp(new MetaNode(var, ch));
    temp->SetWeight(weight);
//    temp->Normalize();
    UniqueTable::iterator it = ut.find(temp);
    if (it != ut.end()) {
        return *it;
    }
    else {
        ut.insert(temp);
        return temp;
    }
}

MetaNodePtr NodeManager::CreateMetaNode(int varid, unsigned int card,
        const vector<ANDNodePtr> &ch, double weight) {
    Scope var;
    var.AddVar(varid, card);
    return CreateMetaNode(var, ch, weight);
}

MetaNodePtr NodeManager::CreateMetaNode(const Scope &vars,
        const vector<double> &vals, double weight) {
    assert(vars.GetCard() == vals.size());
    int rootVarID = vars.GetOrdering().front();
    unsigned int card = vars.GetVarCard(rootVarID);
    Scope rootVar;
    rootVar.AddVar(rootVarID, card);
    vector<ANDNodePtr> children;

    // Need to split the values vector if we are not at a leaf
    if (card != vals.size()) {
        Scope v(vars);
        Scope restVars = v - rootVar;
        vector<vector<double> > valParts = SplitVector(vals, card);
        for (unsigned int i = 0; i < card; i++) {
            vector<MetaNodePtr> ANDch;
            ANDch.push_back(CreateMetaNode(restVars, valParts[i], weight));
            ANDNodePtr newNode(new MetaNode::ANDNode(1.0, ANDch));
            children.push_back(newNode);
        }
    }
    // Otherwise we are at the leaves
    else {
        for (unsigned int i = 0; i < card; i++) {
            const MetaNodePtr &terminal = ((vals[i] == 0) ? MetaNode::GetZero()
                    : MetaNode::GetOne());
            vector<MetaNodePtr> ANDch;
            ANDch.push_back(terminal);
            ANDNodePtr newNode(new MetaNode::ANDNode(vals[i], ANDch));
            children.push_back(newNode);
        }
    }
    MetaNodePtr ret(CreateMetaNode(rootVar, children, weight));
    return ret;
}

vector<MetaNodePtr> NodeManager::FullReduce(MetaNodePtr node, double &w) {
    // terminal check
    if (node.get() == MetaNode::GetZero().get() || node.get()
            == MetaNode::GetOne().get()) {
        return vector<MetaNodePtr> (1, node);
    }
    bool redundant = true;
    const vector<ANDNodePtr> &ch = node->GetChildren();
    ANDNodePtr temp = ch[0];
    for (unsigned int i = 1; i < ch.size(); ++i) {
        if (temp != ch[i]) {
            redundant = false;
            break;
        }
    }

    if (redundant) {
        ut.erase(node);
        vector<MetaNodePtr> andCh = temp->GetChildren();
        vector<MetaNodePtr> ret;
        for (unsigned int i = 0; i < andCh.size(); ++i) {
            vector<MetaNodePtr> reduceSet = FullReduce(andCh[i], w);
            ret.insert(ret.begin(), reduceSet.begin(), reduceSet.end());
        }
        BOOST_FOREACH(MetaNodePtr i, ret) {
            ut.insert(i);
        }
        w = temp->GetWeight();
        return ret;
    }
    else {
        vector<ANDNodePtr> newMetaCh;
        for (unsigned int i = 0; i < ch.size(); ++i) {
            vector<MetaNodePtr> andCh = ch[i]->GetChildren();
            vector<MetaNodePtr> newandCh;
            for (unsigned int j = 0; j < andCh.size(); ++j) {
                vector<MetaNodePtr> reduceSet = FullReduce(andCh[j], w);
                newandCh.insert(newandCh.begin(), reduceSet.begin(),
                        reduceSet.end());
            }
            ANDNodePtr
                    rAND(new MetaNode::ANDNode(w * ch[i]->GetWeight(), newandCh));
            newMetaCh.push_back(rAND);
            w = 1.0;
        }
        node->SetChildren(newMetaCh);
        ut.insert(node);
        return vector<MetaNodePtr>(1, node);
    }

}

MetaNodePtr NodeManager::Apply(MetaNodePtr lhs,
        const vector<MetaNodePtr> &rhs,
        Operator op,
        const DirectedGraph &embeddedPT,
        double w) {
    Operation ocEntry(op, lhs, rhs);
    OperationCache::iterator ocit = opCache.find(ocEntry);
    if ( ocit != opCache.end() ) {
        //Found result in cache
        return ocit->second;
    }

    // Base cases
    switch(op) {
        case PROD:
            if ( lhs.get() == MetaNode::GetZero().get() ) {
                return MetaNode::GetZero();
            }
            else if ( rhs.size() == 0 ) {
                return lhs;
            }
            else {
                for (unsigned int i = 0; i < rhs.size(); ++i) {
                    if ( rhs[i].get() == MetaNode::GetZero().get() ) {
                        return MetaNode::GetZero();
                    }
                }
            }
            if ( lhs.get() == MetaNode::GetOne().get() ) {
                return MetaNode::GetOne();
            }
            break;
        case SUM:
            if ( rhs.size() == 0 ) {
                return lhs;
            }
            if ( lhs.get() == MetaNode::GetOne().get() ) {
                return MetaNode::GetOne();
            }
            break;
        default:
            assert(false);
    }

    int varid = lhs->GetVarID();
    int card = lhs->GetCard();

    vector<ANDNodePtr> children;

    // For each value of lhs
    for (int k = 0; k < card; ++k) {
        // Get original weight
        vector<MetaNodePtr> newChildren;
        double weight = w;
        weight *= lhs->GetChildren()[k]->GetWeight() * lhs->GetWeight();
        vector<MetaNodePtr> lhsChildren =
                lhs->GetChildren()[k]->GetChildren();
        vector<MetaNodePtr> tempChildren;

        if ( rhs.size() == 1 && varid == rhs[0]->GetVarID() ) {
            // Same variable, single roots case
            tempChildren = rhs[0]->GetChildren()[k]->GetChildren();
            switch(op) {
                case PROD:
                    weight *= rhs[0]->GetChildren()[k]->GetWeight() *
	                    rhs[0]->GetWeight();
                    break;
                case SUM:
                    weight += rhs[0]->GetChildren()[k]->GetWeight() *
	                    rhs[0]->GetWeight();

                    // push weights down
                    lhsChildren = ReweighNodes(lhsChildren,
                            lhs->GetChildren()[k]->GetWeight() * lhs->GetWeight());
                    tempChildren = ReweighNodes(tempChildren,
                            rhs[0]->GetChildren()[k]->GetWeight() * lhs->GetWeight());
                    cout << "After adding in APPLY" << endl;
                    cout << weight << endl << endl;
                    break;
                default:
                    assert(false);
            }
        }
        else {
            // Not the same variable, prepare to push rhs down
            tempChildren = rhs;
        }


        // Group nodes into parameter sets for recursive applys
        unordered_set<int> lhsSet;
        unordered_set<int> tempSet;

        // Create maps from the ancestor node to varids.
        unordered_map<int, vector<int> > ancestorMap;

        // Create map from integers to MetaNodes for quick access later
        unordered_map<int, MetaNodePtr> metaNodeMapLeft;
        unordered_map<int, MetaNodePtr> metaNodeMapRight;

        for (unsigned int i = 0; i < lhsChildren.size(); ++i) {
                lhsSet.insert(lhsChildren[i]->GetVarID());
                metaNodeMapLeft.insert(make_pair<int, MetaNodePtr>(
                        lhsChildren[i]->GetVarID(), lhsChildren[i]));
        }
        for (unsigned int i = 0; i < tempChildren.size(); ++i) {
            tempSet.insert(tempChildren[i]->GetVarID());
            metaNodeMapRight.insert(make_pair<int, MetaNodePtr>(
                    tempChildren[i]->GetVarID(), tempChildren[i]));
        }

        // Find highest ancestor within the other set
        unordered_set<int>::iterator lhsIt = lhsSet.begin();
        for (; lhsIt != lhsSet.end(); ++lhsIt) {
            int chvarid = *lhsIt;

            // No need to traverse if determined to be an ancestor
            if (ancestorMap.find(chvarid) != ancestorMap.end()) {
                continue;
            }

            if (chvarid == -1) {
                ancestorMap[-1].push_back(-1);
                continue;
            }

            // Check pseudo tree
            DInEdge ei, ei_end;
            tie(ei, ei_end) = in_edges(chvarid, embeddedPT);

            int highestAncestor = chvarid;
            int count = 0; // Debugging use
            while (ei != ei_end) {
                assert(++count <= 1);
                int parent = source(*ei, embeddedPT);
                if ( tempSet.find(parent) != tempSet.end() ) {
                    highestAncestor = parent;
                    tie(ei, ei_end) = in_edges(parent, embeddedPT);
                    count = 0;
                }
                ++ei;
            }
            ancestorMap[highestAncestor].push_back(chvarid);
        }

        // Repeat from other side
        unordered_set<int>::iterator tempIt = tempSet.begin();
        for (; tempIt != tempSet.end(); ++tempIt) {
            int chvarid = *tempIt;

            // No need to traverse if determined to be an ancestor
            if (ancestorMap.find(chvarid) != ancestorMap.end()) {
                continue;
            }

            if (chvarid == -1) {
                ancestorMap[-1].push_back(-1);
                continue;
            }

            // Check pseudo tree
            DInEdge ei, ei_end;
            tie(ei, ei_end) = in_edges(chvarid, embeddedPT);
            int highestAncestor = chvarid;
            while (ei != ei_end) {
                int parent = source(*ei, embeddedPT);
                if ( tempSet.find(parent) != tempSet.end() ) {
                    highestAncestor = parent;
                    tie(ei, ei_end) = in_edges(parent, embeddedPT);
                }
                ++ei;
            }
            ancestorMap[highestAncestor].push_back(chvarid);
        }

        vector< pair<MetaNodePtr, vector<MetaNodePtr> > > paramSets;
        unordered_map<int, vector<int> >::iterator ait = ancestorMap.begin();

        // Display ANCESTOR MAP
        cout << "ANCESTOR MAP" << endl;
        for (; ait != ancestorMap.end(); ++ait) {
            cout << "Variable: "<< ait->first << endl;
            PrintVector(ait->second, cout);
            cout << endl;
        }

        //==========
        ait = ancestorMap.begin();
        unordered_map<int,MetaNodePtr>::iterator mit;
        for (; ait != ancestorMap.end(); ++ait) {
            vector<MetaNodePtr> paramRHS;
            const vector<int> &descendants = ait->second;

            // Find a suitable left hand parameter
            mit = metaNodeMapLeft.find(ait->first);
            MetaNodePtr temp;
            if (mit == metaNodeMapLeft.end()) {
                mit = metaNodeMapRight.find(ait->first);
                temp = mit->second;
                metaNodeMapRight.erase(mit);
            }
            else {
                temp = mit->second;
                metaNodeMapLeft.erase(mit);
            }

            // Build up right hand parameter
            for (unsigned int i = 0; i < descendants.size(); ++i) {
                mit = metaNodeMapRight.find(descendants[i]);
                if (mit == metaNodeMapRight.end()) {
                    mit = metaNodeMapLeft.find(descendants[i]);
                    if (mit != metaNodeMapLeft.end()) {
                        paramRHS.push_back(mit->second);
                        metaNodeMapLeft.erase(mit);
                    }
                }
                else {
                    paramRHS.push_back(mit->second);
                    metaNodeMapRight.erase(mit);
                }
//                if (ait->first != descendants[i]) {
//                }
            }
            paramSets.push_back(make_pair<MetaNodePtr, vector<MetaNodePtr> >(
                    temp, paramRHS));
        }

        // For each parameter set
        for (unsigned int i = 0; i < paramSets.size(); ++i) {
            cout << endl;
            cout << "ParamSet size:" << paramSets.size() << endl;
            cout << "lhs: " << endl;
            paramSets[i].first->Save(cout); cout << endl << endl;
            cout << "rhs: " << endl;
            for (unsigned int j = 0; j < paramSets[i].second.size(); ++j) {
                paramSets[i].second[j]->Save(cout); cout << endl << endl;
            }
            cout << "End of param set" << endl << endl;
            MetaNodePtr subDD = Apply(paramSets[i].first, paramSets[i].second, op, embeddedPT, w);
            if ( subDD == MetaNode::GetZero() ) {
                newChildren.push_back(MetaNode::GetZero());
                break;
            }
            else {
                newChildren.push_back(subDD);
            }
            if (op == SUM &&
                    subDD.get() != MetaNode::GetZero().get() &&
                    subDD.get() != MetaNode::GetOne().get()) {
                weight = 1;
            }
        }
        ANDNodePtr newNode(new MetaNode::ANDNode(weight, newChildren));
        children.push_back(newNode);
    }
    // Redundancy can be resolved outside
    Scope var;
    var.AddVar(varid, card);
    MetaNodePtr u = CreateMetaNode(var, children);
    Operation entryKey(op, lhs, rhs);
    opCache.insert(make_pair<Operation, MetaNodePtr>(entryKey, u));
    return u;
}

// Sets each AND node of variables to marginalize to be the result of summing
// the respective MetaNode children of each AND node. Redundancy can be
// resolved outside.
MetaNodePtr NodeManager::Marginalize(MetaNodePtr root, const Scope &s,
        const DirectedGraph &embeddedpt) {
    if (root.get() == MetaNode::GetZero().get()) {
        return MetaNode::GetZero();
    }
    else if (root.get() == MetaNode::GetOne().get()) {
        return MetaNode::GetOne();
    }
    int varid = root->GetVarID();
    int card = root->GetCard();

    // Marginalize each subgraph
    const vector<ANDNodePtr> &andNodes = root->GetChildren();
    vector<ANDNodePtr> newANDNodes;
    BOOST_FOREACH(ANDNodePtr i, andNodes) {
        const vector<MetaNodePtr> &metaNodes = i->GetChildren();
        vector<MetaNodePtr> newMetaNodes;
        BOOST_FOREACH(MetaNodePtr j, metaNodes) {
            newMetaNodes.push_back(Marginalize(j, s, embeddedpt));
        }
        ANDNodePtr newANDNode(new MetaNode::ANDNode(i->GetWeight(), newMetaNodes));
        newANDNodes.push_back(newANDNode);
    }

    // If the root is to be marginalized
    if (s.VarExists(varid)) {
        // Make a copy for the first value
        const vector<MetaNodePtr> &firstMetaNodes = newANDNodes[0]->GetChildren();
        vector<MetaNodePtr> newMetaNodes;
        // Propagate the and weight downward
        BOOST_FOREACH(MetaNodePtr i, firstMetaNodes) {
            MetaNodePtr reweightedNode(new MetaNode(*i));
            reweightedNode->SetWeight(i->GetWeight() * newANDNodes[0]->GetWeight());
            newMetaNodes.push_back(reweightedNode);
        }

        for (unsigned int i = 1; i < newANDNodes.size(); ++i) {
            assert(i<2);
            const vector<MetaNodePtr> &curMetaNodes = newANDNodes[i]->GetChildren();
            vector<MetaNodePtr> tempMetaNodes;
            // Propagate the and weight downward
            BOOST_FOREACH(MetaNodePtr m, curMetaNodes) {
                MetaNodePtr reweightedNode(new MetaNode(*m));
                reweightedNode->SetWeight(m->GetWeight() * newANDNodes[i]->GetWeight());
                tempMetaNodes.push_back(reweightedNode);
            }

            // "+=" on newMetaNodes
            Scope s;
            s.AddVar(3,2);
            s.AddVar(6,2);
            Assignment a(s);
            a.SetAllVal(0);
            for (unsigned int j = 0; j < newMetaNodes.size(); ++j) {
                assert(j < 1);
                vector<MetaNodePtr> rhsParam(1, tempMetaNodes[j]);
                cout << "LHS:" << endl;
                do {
                    a.Save(cout); cout << " value = " << newMetaNodes[j]->Evaluate(a) << endl;
                } while (a.Iterate());
                cout << "RHS:" << endl;
                do {
                    a.Save(cout); cout << " value = " << rhsParam[0]->Evaluate(a) << endl;
                } while (a.Iterate());
                MetaNodePtr newMeta = Apply(newMetaNodes[j], rhsParam, SUM, embeddedpt);
//                cout << "Intermediate step:" << endl;
//                newMeta->RecursivePrint(cout); cout << endl;
                cout << "Summed" << endl;
                do {
                    a.Save(cout); cout << " value = " << newMeta->Evaluate(a) << endl;
                } while (a.Iterate());
                newMetaNodes[j] = newMeta;
            }
        }
        newANDNodes.clear();
        ANDNodePtr newAND(new MetaNode::ANDNode(1, newMetaNodes));
        for (unsigned int i = 0; i < andNodes.size(); ++i) {
            newANDNodes.push_back(newAND);
        }
    }
    Scope var;
    var.AddVar(varid, card);
    MetaNodePtr ret = CreateMetaNode(var, newANDNodes, root->GetWeight());
    return ret;
}
unsigned int NodeManager::GetNumberOfNodes() const {
    return ut.size();
}

void NodeManager::PrintUniqueTable(ostream &out) const {
    BOOST_FOREACH (MetaNodePtr i, ut) {
        i->Save(out); out << endl;
    }
}

bool NodeManager::initialized = false;
NodeManager *NodeManager::singleton = NULL;

} // end of aomdd namespace
