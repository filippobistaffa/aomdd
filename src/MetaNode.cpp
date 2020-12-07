/*
 *  MetaNode.cpp
 *  aomdd
 *
 *  Created by William Lam on May 10, 2011
 *  Copyright 2011 UC Irvine. All rights reserved.
 *
 */

#include "MetaNode.h"
#include "NodeManager.h"
#include <sstream>


namespace aomdd {

using namespace std;

MetaNode::ANDNode::ANDNode() :
    weight(1), refs(0) {
}

MetaNode::ANDNode::ANDNode(double w, const vector<MetaNodePtr> &ch) :
    weight(w), children(ch), refs(0) {
}

MetaNode::ANDNode::~ANDNode() {
    BOOST_FOREACH(MetaNodePtr &i, children) {
        i->RemoveParent(this);
        i.reset();
    }
}

double MetaNode::ANDNode::Evaluate(const Assignment &a) {
    double ret = weight;
    BOOST_FOREACH(const MetaNodePtr &i, children) {
        ret *= i->Evaluate(a);
        if (ret == 0)
            return ret;
    }
    return ret;
}

double MetaNode::ANDNode::GetWeight() const {
    return weight;
}

void MetaNode::ANDNode::SetWeight(double w) {
    weight = w;
}

void MetaNode::ANDNode::ScaleWeight(double w) {
    weight *= w;
}

void MetaNode::ANDNode::SetChildren(const std::vector<MetaNodePtr> &ch) {
    children = ch;
}

void MetaNode::ANDNode::Save(ostream &out, string prefix) const {
    out << prefix << "and-id: " << this << endl;
    out << prefix << "weight: " << weight << endl;
    out << prefix << "children: ";
    BOOST_FOREACH(const MetaNodePtr &i, children) {
        out << " " << i;
    }
}

void MetaNode::ANDNode::RecursivePrint(ostream &out, string prefix) const {
    Save(out, prefix); out << endl;
    BOOST_FOREACH(const MetaNodePtr &i, children) {
        i->RecursivePrint(out, prefix + "    ");
        out << endl;
    }
}

void MetaNode::ANDNode::RecursiveGenerateDot(google::sparse_hash_map<size_t,int> &nodes, int &currentNodeId, AOGraph &g) const {
    BOOST_FOREACH(const MetaNodePtr &i, children) {
        if (NodeManager::GetNodeManager()->GetCMOnly() && i->IsTerminal()) continue;
        if (nodes.find(size_t(i.get())) == nodes.end()) {
            nodes.insert(make_pair<size_t,int>(size_t(i.get()),currentNodeId++));
            add_vertex(AOVertexProp(i->GetVarID(),OR),g);
            i->RecursiveGenerateDot(nodes,currentNodeId,g);
        }
        add_edge(nodes[size_t(this)],nodes[size_t(i.get())],AOEdgeProp(1,ANDtoOR),g);
    }
}

void MetaNode::ANDNode::GenerateDiagram(DirectedGraph &diagram, const DVertexDesc &parent) const {
    stringstream ss;
    ss << this;
    string cur = ss.str();
    ss.clear();
    DVertexDesc current = add_vertex(cur, diagram);
    add_edge(parent, current, diagram);
    BOOST_FOREACH(const MetaNodePtr &i, children) {
        i->GenerateDiagram(diagram, current);
    }
}

bool operator==(const ANDNodePtr &lhs, const ANDNodePtr &rhs) {
    if (lhs.get() == rhs.get()) return true;
    if (NodeManager::GetNodeManager()->GetCMOnly() || 
            fabs(lhs->GetWeight() - rhs->GetWeight()) >= TOLERANCE ||
            lhs->GetChildren().size() != rhs->GetChildren().size() ||
            lhs->GetChildren() != rhs->GetChildren()) {
        return false;
    }
    /*
    for (unsigned int i = 0; i < lhs->GetChildren().size(); i++) {
        if (lhs->GetChildren()[i].get() != rhs->GetChildren()[i].get()) {
            return false;
        }
    }
    */
    return true;
}

bool operator!=(const ANDNodePtr &lhs, const ANDNodePtr &rhs) {
    return !(lhs == rhs);
}

// ======================

MetaNode::MetaNode() : varID(-1), card(0), elimValueCached(false), refs(0) {
}

MetaNode::MetaNode(const Scope &var, const vector<ANDNodePtr> &ch) :
    children(ch), elimValueCached(false), refs(0) {
    // Scope must be over one variable
    assert(var.GetNumVars() == 1);
    varID = var.GetOrdering().front();
    card = var.GetVarCard(varID);

    // All assignments must be specified
    hashVal = hash_value(*this);
    assert(var.GetCard() == children.size() || children.size() == 1);
}

MetaNode::MetaNode(int varidIn, int cardIn, const vector<ANDNodePtr> &ch) :
    varID(varidIn), card(cardIn), children(ch), elimValueCached(false), refs(0) {
    // Scope must be over one variable
    // All assignments must be specified
    hashVal = hash_value(*this);
    assert(card == children.size() || children.size() == 1);
}

MetaNode::~MetaNode() {
    BOOST_FOREACH(ANDNodePtr &i, children) {
        i.reset();
    }
}

double MetaNode::Normalize() {
    if (IsTerminal()) {
        return 1.0;
    }
    double normValue = 0;
    BOOST_FOREACH(ANDNodePtr &i, children) {
        normValue += i->GetWeight();
    }
    BOOST_FOREACH(ANDNodePtr &i, children) {
        i->SetWeight(i->GetWeight() / normValue);
    }
    return normValue;
}

double MetaNode::Maximum(const Assignment &a) {
    int val;
    if (this == MetaNode::GetZero().get()) {
        return 0.0;
    }
    else if (this == MetaNode::GetOne().get()) {
        return 1.0;
    }
    else if (elimValueCached) {
        return cachedElimValue;
    }
    else if ( (val = a.GetVal(varID)) != ERROR_VAL) {
        double temp = children[val]->GetWeight();
        BOOST_FOREACH(MetaNodePtr j, children[val]->GetChildren()) {
            temp *= j->Maximum(a);
        }
        cachedElimValue = temp;
        elimValueCached = true;
    }
    else {
        double maxVal = DOUBLE_MIN;
        BOOST_FOREACH(ANDNodePtr i, children) {
            double temp = i->GetWeight();
            BOOST_FOREACH(MetaNodePtr j, i->GetChildren()) {
                temp *= j->Maximum(a);
            }
            if (temp > maxVal) maxVal = temp;
        }
        cachedElimValue = maxVal;
        elimValueCached = true;
    }
    return cachedElimValue;
}

double MetaNode::Sum(const Assignment &a) {
    int val;
    if (this == MetaNode::GetZero().get()) {
        return 0.0;
    }
    else if (this == MetaNode::GetOne().get()) {
        return 1.0;
    }
    else if (elimValueCached) {
        return cachedElimValue;
    }
    else if ( (val = a.GetVal(varID)) >= 0 ) {
        double temp = children[val]->GetWeight();
        BOOST_FOREACH(MetaNodePtr j, children[val]->GetChildren()) {
            temp *= j->Sum(a);
        }
        cachedElimValue = temp;
        elimValueCached = true;
    }
    else {
        double total = 0.0;
        BOOST_FOREACH(ANDNodePtr i, children) {
            double temp = i->GetWeight();
            BOOST_FOREACH(MetaNodePtr j, i->GetChildren()) {
                temp *= j->Sum(a);
            }
            total += temp;
        }
        cachedElimValue = total;
        elimValueCached = true;
    }
    return cachedElimValue;
}

double MetaNode::Evaluate(const Assignment &a) const {
    if (this == GetZero().get()) {
        return 0;
    }
    else if (this == GetOne().get()) {
        return 1;
    }
    /*
    // Handle dummy variable case
    else if (card == 1) {
        return children[0]->Evaluate(a);
    }
    */
    else {
        int idx = a.GetVal(varID);
        assert(idx >= 0);
        return children[idx]->Evaluate(a);
    }
}

void MetaNode::Save(ostream &out, string prefix) const {
    if(this == MetaNode::GetZero().get()) {
        out << prefix << "TERMINAL ZERO" << endl;
    }
    else if(this == MetaNode::GetOne().get()) {
        out << prefix << "TERMINAL ONE" << endl;
    }
    else {
        out << prefix << "varID: " << varID;
        if (IsDummy()) {
            out << " (dummy)";
        }
    }
    out << endl;
    out << prefix << "id: " << this << endl;
//    out << prefix << "weight: " << weight << endl;
    out << prefix << "parent ids: ";
    BOOST_FOREACH(ANDNode* i, parents) {
        out << " " << i;
    }
    out << endl;
    out << prefix << "children: ";
    BOOST_FOREACH(const ANDNodePtr &i, children) {
        out << " " << i;
    }
    if (children.empty()) {
        out << "None";
    }
}

void MetaNode::RecursivePrint(ostream &out, string prefix) const {
    Save(out, prefix); out << endl;
    BOOST_FOREACH(const ANDNodePtr &i, children) {
        i->RecursivePrint(out, prefix + "    ");
        out << endl;
    }
}

void MetaNode::RecursivePrint(ostream &out) const {
    RecursivePrint(out, "");
}

void MetaNode::RecursiveGenerateDot(google::sparse_hash_map<size_t,int> &nodes, int &currentNodeId, AOGraph &g) const {
    for (unsigned i = 0; i < children.size(); ++i) {
        nodes.insert(make_pair<size_t,int>(size_t(children[i].get()),currentNodeId++));
        add_vertex(AOVertexProp(i,AND),g);
        add_edge(nodes[size_t(this)],nodes[size_t(children[i].get())],
                AOEdgeProp(children[i]->GetWeight(),ORtoAND),g);
        children[i]->RecursiveGenerateDot(nodes,currentNodeId,g);
    }
}

DirectedGraph MetaNode::GenerateDiagram() const {
    DirectedGraph diagram;
    DVertexDesc dummy(VertexDesc(0));
    GenerateDiagram(diagram, dummy);
    return diagram;
}

void MetaNode::GenerateDiagram(DirectedGraph &diagram, const DVertexDesc &parent) const {
    stringstream ss;
    ss << this;
    string cur = ss.str();
    ss.clear();
    DVertexDesc current = add_vertex(cur, diagram);
    add_edge(parent, current, diagram);
    BOOST_FOREACH(const ANDNodePtr &i, children) {
        i->GenerateDiagram(diagram, current);
    }
}

pair<unsigned int, unsigned int> MetaNode::NumOfNodes() const {
    boost::unordered_set<const MetaNode *> nodeSet;
    FindUniqueNodes(nodeSet);
    unsigned int count = 0;
    BOOST_FOREACH(const MetaNode *m, nodeSet) {
        count += m->GetCard();
    }
    return pair<unsigned int, unsigned int>(nodeSet.size(), count);
}

void MetaNode::GetNumNodesPerVar(vector<unsigned int> &numMeta) const {
    boost::unordered_set<const MetaNode *> nodeSet;
    FindUniqueNodes(nodeSet);
    BOOST_FOREACH(const MetaNode *m, nodeSet) {
        int vid = m->GetVarID();
        assert(vid < int(numMeta.size()));
        numMeta[vid]++;
    }
}

double MetaNode::ComputeTotalMemory() const {
    boost::unordered_set<const MetaNode *> nodeSet;
    FindUniqueNodes(nodeSet);
    double memUsage = 0;
    BOOST_FOREACH(const MetaNode *m, nodeSet) {
        memUsage += m->MemUsage();
    }
    return memUsage;
}

void MetaNode::FindUniqueNodes(boost::unordered_set<const MetaNode *> &nodeSet) const {
    if (IsTerminal()) {
        return;
    }
    unsigned int oldSize = nodeSet.size();
    nodeSet.insert(this);

    // Check if this has already been visited
    if (nodeSet.size() == oldSize) {
//        cout << "Found something isomorphic! (at var " << varID << ")" << endl;
        return;
    }
    BOOST_FOREACH(const ANDNodePtr &i, children) {
        BOOST_FOREACH(const MetaNodePtr &j, i->GetChildren()) {
            j->FindUniqueNodes(nodeSet);
        }
    }
}

bool MetaNode::IsRedundant() const {
    if (IsTerminal()) return false;
    bool redundant = true;
    ANDNodePtr temp = children[0];
    for (unsigned int i = 1; i < children.size(); ++i) {
        if (temp != children[i]) {
            redundant = false;
            break;
        }
    }
    return redundant;
}

void MetaNode::SetChildrenParent(MetaNodePtr m) {
    BOOST_FOREACH(ANDNodePtr i, children) {
        i->SetParent(m);
    }
}


// Initialize static variables
MetaNodePtr MetaNode::terminalZero;
MetaNodePtr MetaNode::terminalOne;
bool MetaNode::zeroInit = false;
bool MetaNode::oneInit = false;

} // end of aomdd namespace
