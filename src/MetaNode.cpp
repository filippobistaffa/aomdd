/*
 *  MetaNode.cpp
 *  aomdd
 *
 *  Created by William Lam on May 10, 2011
 *  Copyright 2011 UC Irvine. All rights reserved.
 *
 */

#include "MetaNode.h"

namespace aomdd {
using namespace std;

MetaNode::ANDNode::ANDNode() :
    weight(0) {
}

MetaNode::ANDNode::ANDNode(double w, const vector<MetaNodePtr> &ch) :
    weight(w), children(ch) {
}

MetaNode::ANDNode::~ANDNode() {
}

double MetaNode::ANDNode::Evaluate(const Assignment &a) {
    double ret = weight;
    BOOST_FOREACH(MetaNodePtr i, children)
                {
                    ret *= i->Evaluate(a);
                    if (ret == 0)
                        return ret;
                }
    return ret;
}

double MetaNode::ANDNode::GetWeight() const {
    return weight;
}

const vector<MetaNodePtr> &MetaNode::ANDNode::GetChildren() const {
    return children;
}

// Used mainly to see if all ANDNodes in a vector are the same (to handle
// redundancy)
bool MetaNode::ANDNode::operator==(const ANDNode &rhs) const {
    if (weight != rhs.weight || children.size() != rhs.children.size()) {
        return false;
    }
    for (unsigned int i = 0; i < children.size(); i++) {
        if (children[i].get() != rhs.children[i].get()) {
            return false;
        }
    }
    return true;
}

void MetaNode::ANDNode::Save(ostream &out) {
    out << "id: " << this << endl;
    out << "weight: " << weight << endl;
    out << "children: ";
    BOOST_FOREACH(MetaNodePtr i, children)
                {
                    out << " " << i;
                }
}

bool operator==(const ANDNodePtr &lhs, const ANDNodePtr &rhs) {
    if (lhs->GetWeight() != rhs->GetWeight() || lhs->GetChildren().size()
            != rhs->GetChildren().size()) {
        return false;
    }
    for (unsigned int i = 0; i < lhs->GetChildren().size(); i++) {
        if (lhs->GetChildren()[i].get() != rhs->GetChildren()[i].get()) {
            return false;
        }
    }
    return true;

}

// ======================

MetaNode::MetaNode() {
}

MetaNode::MetaNode(const Scope &var, const vector<ANDNodePtr> &ch) :
    s(&var), children(ch) {
    // Scope must be over one variable
    assert(s->GetNumVars() == 1);
    // All assignments must be specified
    assert(var.GetCard() == children.size());
}

MetaNode::~MetaNode() {
}

double MetaNode::Evaluate(const Assignment &a) const {
    if (this == GetZero().get()) {
        return 0;
    }
    else if (this == GetOne().get()) {
        return 1;
    }
    else {
        unsigned int idx = a.GetVal(s->GetOrdering().front());
        return children[idx]->Evaluate(a);
    }
}

const vector<ANDNodePtr> &MetaNode::GetChildren() const {
    return children;
}

// Considered equal if the scope and children pointers are the same
bool MetaNode::operator==(const MetaNode &rhs) const {
    if (this == GetZero().get() || this == GetOne().get())
        return false;
    if (s != rhs.s || children.size() != rhs.children.size())
        return false;
    for (unsigned int i = 0; i < children.size(); i++) {
        if (children[i] != rhs.children[i])
            return false;
    }
    return true;
}

void MetaNode::Save(ostream &out) {
    out << "id: " << this << endl;
    out << "children: ";
    BOOST_FOREACH(ANDNodePtr i, children)
                {
                    out << " " << i;
                }
    if (children.empty()) {
        out << "None";
    }
}

const MetaNodePtr &MetaNode::GetZero() {
    if (!zeroInit) {
        terminalZero = MetaNodePtr(new MetaNode());
        zeroInit = true;
        return terminalZero;
    }
    else {
        return terminalZero;
    }
}

const MetaNodePtr &MetaNode::GetOne() {
    if (!oneInit) {
        terminalOne = MetaNodePtr(new MetaNode());
        oneInit = true;
        return terminalOne;
    }
    else {
        return terminalOne;
    }
}

size_t hash_value(const MetaNode &node) {
    size_t seed = 0;
    boost::hash_combine(seed, node.s);
    BOOST_FOREACH(ANDNodePtr i, node.children)
                {
                    boost::hash_combine(seed, i.get());
                }
    return seed;

}

size_t hash_value(const MetaNodePtr &node) {
    size_t seed = 0;
    boost::hash_combine(seed, node->GetScopePtr());
    BOOST_FOREACH(ANDNodePtr i, node->GetChildren())
                {
                    boost::hash_combine(seed, i.get());
                }
    return seed;
}

bool operator==(const MetaNodePtr &lhs, const MetaNodePtr &rhs) {
    cout << "Equality Checker Called" << endl;
    if (lhs.get() == MetaNode::GetZero().get() && rhs.get()
            == MetaNode::GetOne().get())
        return false;
    if (rhs.get() == MetaNode::GetZero().get() && lhs.get()
            == MetaNode::GetOne().get())
        return false;
    if (lhs->GetScopePtr() != rhs->GetScopePtr() || lhs->GetChildren().size()
            != rhs->GetChildren().size())
        return false;
    for (unsigned int i = 0; i < lhs->GetChildren().size(); i++) {
        if (lhs->GetChildren()[i].get() != rhs->GetChildren()[i].get())
            return false;
    }
    return true;
}

// Initialize static variables
MetaNodePtr MetaNode::terminalZero;
MetaNodePtr MetaNode::terminalOne;
bool MetaNode::zeroInit = false;
bool MetaNode::oneInit = false;


} // end of aomdd namespace
