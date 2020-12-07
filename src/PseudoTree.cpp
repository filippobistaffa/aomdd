/*
 *  PseudoTree.cpp
 *  aomdd
 *
 *  Created by William Lam on Jun 21, 2011
 *  Copyright 2011 UC Irvine. All rights reserved.
 *
 */

#include "PseudoTree.h"
#include <stack>
#include <set>

namespace aomdd {
using namespace std;

PseudoTree::PseudoTree() {
}

PseudoTree::PseudoTree(const Model &m) {
    Graph g(m.GetNumVars(), m.GetScopes());
    cout << "Inducing edges" << endl;
    g.InduceEdges(m.GetOrdering());
    cout << "...done" << endl;
    inducedWidth = g.GetInducedWidth();
    s = m.GetCompleteScope();
    context.resize(s.GetNumVars());
    cout << "Performing DFS building" << endl;
    DFSGenerator(g);
    cout << "...done" << endl;
//    ComputeContext(g);
    if (hasDummy) {
        s.AddVar(root, 1);
    }
}

PseudoTree::PseudoTree(const Graph &inducedGraph, const Scope &sIn, bool chainStructure)
: inducedWidth(inducedGraph.GetInducedWidth()), s(sIn), hasDummy(false) {
    context.resize(s.GetNumVars());
    if (!chainStructure)
        DFSGenerator(inducedGraph);
    else
        ChainGenerator(inducedGraph);
    ComputeContext(inducedGraph);
    if (hasDummy) {
        s.AddVar(root, 1);
    }
}

int PseudoTree::GetNumberOfNodes() const {
    return num_vertices(g);
}

int PseudoTree::GetInducedWidth() const {
    assert(inducedWidth != -1);
    return inducedWidth;
}

unsigned int PseudoTree::GetHeight() const {
    if (num_vertices(g) <= 1) return 0;
    return GetHeight(root) - hasDummy;
}

unsigned int PseudoTree::GetHeight(int r) const {
    DEdge out, out_end;
    tie(out, out_end) = out_edges(r, g);
    if (out == out_end) {
        return 0;
    }
    int subtreeMax = -1;
    for (; out != out_end; ++out) {
       int subtreeHeight = GetHeight(target(*out, g));
       if (subtreeHeight > subtreeMax) {
           subtreeMax = subtreeHeight;
       }
    }
    return 1 + subtreeMax;
}

void PseudoTree::ComputeContext(const Graph &inducedGraph) {
    set<int> anc;
    if (hasDummy) {
        DEdge out, out_end;
        tie(out, out_end) = out_edges(root, g);
        while (out != out_end) {
            ComputeContext(target(*out, g), inducedGraph, anc);
            out++;
        }
    }
    else {
        ComputeContext(root, inducedGraph, anc);
    }
}

const set<int> &PseudoTree::ComputeContext(int r, const Graph &inducedGraph, set<int> &ancestors) {

    // See if context has already been computed
    if (!context[r].empty()) {
        ancestors.erase(r);
        return context[r];
    }

    DEdge out, out_end;
    tie(out, out_end) = out_edges(r, g);
    const UndirectedGraph &ig = inducedGraph.GetGraph();

    // Leaf node case
    if (out == out_end) {
        Edge ei, ei_end;
        tie(ei, ei_end) = out_edges(r, ig);
        while (ei != ei_end) {
           context[r].insert(target(*ei, ig));
           ei++;
        }
    }

    // Get union of contexts of children
    else {
        ancestors.insert(r);
        while (out != out_end) {
            const set<int> &temp = ComputeContext(target(*out, g), inducedGraph, ancestors);
            BOOST_FOREACH(int v, temp) {
                context[r].insert(v);
            }
            // also add in its ancestors
            context[r].erase(r);
            out++;
        }
        Edge ei, ei_end;
        tie(ei, ei_end) = out_edges(r, ig);
        while (ei != ei_end) {
            int neighbor = target(*ei, ig);
            if (ancestors.find(neighbor) != ancestors.end()) {
                context[r].insert(neighbor);
            }
            ei++;
        }

        if (context[r].empty() ) {
            context[r].insert(-1);
        }
    }
    ancestors.erase(r);
    return context[r];
}

void PseudoTree::DFSGenerator(const Graph &inducedGraph) {
    using namespace boost;

    UndirectedGraph ig(inducedGraph.GetGraph());
    vector<int> component(num_vertices(ig));
    root = inducedGraph.GetRoot();
    int numOfComponents = connected_components(ig, &component[0]);

    list<int> ordering = inducedGraph.GetOrdering();
    if (numOfComponents > 1) {
        hasDummy = true;
        // Make a dummy root
        root = component.size();
        // Connect components to dummy root
        vector<bool> connected(numOfComponents, false);
        list<int>::const_iterator it = ordering.begin();
        for (; it != ordering.end(); ++it) {
            if (!connected[component[*it]]) {
                add_edge(root, *it, ig);
                connected[component[*it]] = true;
            }
        }
        ordering.push_front(root);
    }

    vector<bool> visited(ordering.size(), false);
    stack<pair<int,int> > stk;
    stk.push(make_pair(-1, root));

    Edge ei, ei_end;

    while (!stk.empty()) {
        int u = stk.top().first;
        int v = stk.top().second; 
        stk.pop();
        if (!visited[v]) {
            if (u >= 0) add_edge(u, v, g);
            visited[v] = true;
            set<int> adj;
            boost::tie(ei, ei_end) = out_edges(v, ig);
            for (; ei != ei_end; ++ei) {
                adj.insert(target(*ei, ig));
            }

            list<int>::const_reverse_iterator rit = ordering.rbegin();
            for (; rit != ordering.rend(); ++rit) {
                if (adj.count(*rit)) {
                    stk.push(make_pair(v,*rit));
                }
            }
        }
    }
}

void PseudoTree::ChainGenerator(const Graph &inducedGraph) {
    list<int> ordering = inducedGraph.GetOrdering();
    list<int>::iterator it = ordering.begin();
    int current = root = *it;
    int previous;
    ++it;
    for (; it != ordering.end(); ++it) {
        previous = current;
        current = *it;
        add_edge(previous,current,g);
    }
}


// To finish later
void PseudoTree::BalancingGenerator(const Graph &inducedGraph) {
    // First, see if the graph has > 1 connected component
    UndirectedGraph ig(inducedGraph.GetGraph());
    int numVertices = num_vertices(ig);

    // For each node, find node to remove that best reduces the
    // max component size
    for (int i = 0; i < numVertices; ++i) {
        vector<int> component(num_vertices(ig));
//        int numComponents = connected_components(ig, &component[0]);
    }
}

pair<DirectedGraph, int> PseudoTree::GenerateEmbeddable(const Scope &s) const {
    using namespace boost;
    DirectedGraph embeddableTree;
    int embedRoot = -1;
    EmbedTreeGenerator vis(embeddableTree, s, embedRoot);
    depth_first_search(g, root_vertex(VertexDesc(root)).
            visitor(vis).
            edge_color_map(get(edge_color,g)));
    return make_pair(embeddableTree, embedRoot);
}

PseudoTree::~PseudoTree() {
}

} // end of aomdd namespace

