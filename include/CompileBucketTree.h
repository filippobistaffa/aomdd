/*
 *  CompileBucketTree.h
 *  aomdd
 *
 *  Created by William Lam on Jul 15, 2011
 *  Copyright 2011 UC Irvine. All rights reserved.
 *
 */

#ifndef COMPILEBUCKETTREE_H_
#define COMPILEBUCKETTREE_H_

#include "base.h"
#include "Model.h"
#include "CompileBucket.h"
#include "PseudoTree.h"

namespace aomdd {

class CompileBucketTree {
    std::vector<CompileBucket> buckets;
    const PseudoTree *pt;
    std::list<int> ordering;

    bool fullReduce;
    bool compiled;
    AOMDDFunction compiledDD;
public:
    CompileBucketTree();
    CompileBucketTree(const Model &m, const PseudoTree *ptIn, const std::list<int> &orderIn, bool fr=true);

    AOMDDFunction Compile();

    double Prob(bool logOut = false);

    void PrintBucketFunctionScopes(std::ostream &out) const;
    virtual ~CompileBucketTree();
};

}

#endif /* COMPILEBUCKETTREE_H_ */
