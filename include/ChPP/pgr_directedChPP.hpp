/*PGR-GNU*****************************************************************

Copyright (c) 2015 pgRouting developers
Mail: project@pgrouting.org

Copyright (c) 2018 Maoguang Wang 
Mail: xjtumg1007@gmail.com

------

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

********************************************************************PGR-GNU*/

#ifndef INCLUDE_CHPP_PGR_DIRECTEDCHPP_HPP_
#define INCLUDE_CHPP_PGR_DIRECTEDCHPP_HPP_
#pragma once

#include "costFlow/pgr_minCostMaxFlow.hpp"
#include "c_types/general_path_element_t.h"
#include "c_types/pgr_edge_t.h"
#include "c_types/pgr_flow_t.h"

namespace pgrouting {
namespace graph {

class PgrDirectedChPPGraph {
 public:
     PgrDirectedChPPGraph(
             const pgr_edge_t *dataEdges,
             const size_t totalEdges);

     double DirectedChPP() {
         double minAddedCost = flowGraph.MinCostMaxFlow();
         int64_t maxFlow = flowGraph.GetMaxFlow();
         if (maxFlow == totalDeg)
             return minAddedCost + totalCost;
         return -1.0;
     }

     std::vector<General_path_element_t> GetPathEdges();

 private:
     bool EulerCircuitDFS(int64_t p, std::vector<size_t>::iterator edgeToFaIter);
     void BuildResultGraph();

 private:
     int64_t totalDeg;
     double totalCost;
     int64_t superSource, superTarget;
     int64_t startPoint;

     graph::PgrCostFlowGraph flowGraph;
     std::vector<pgr_edge_t> resultEdges;

     std::vector<std::pair<int64_t, std::vector<size_t> > > resultGraph;
     std::map<int64_t, size_t> VToVecid;
     std::vector<bool> edgeVisited;

     std::vector<General_path_element_t> resultPath;
};

PgrDirectedChPPGraph::PgrDirectedChPPGraph(
        const pgr_edge_t *dataEdges,
        const size_t totalEdges) {
    resultEdges.clear();
    for (size_t i = 0; i < totalEdges; i++) {
        pgr_edge_t edge;
        edge.id = dataEdges[i].id;
        edge.source = dataEdges[i].source;
        edge.target = dataEdges[i].target;
        edge.reverse_cost = -1.0;
        if (dataEdges[i].cost > 0) {
            startPoint = edge.source;
            edge.cost = dataEdges[i].cost;
            totalCost += edge.cost;
            resultEdges.push_back(edge);
        }
        if (dataEdges[i].reverse_cost > 0) {
            std::swap(edge.source, edge.target);
            edge.cost = dataEdges[i].reverse_cost;
            totalCost += edge.cost;
            resultEdges.push_back(edge);
        }
    }

    std::vector<pgr_costFlow_t> edges;
    std::set<int64_t> sources;
    std::set<int64_t> targets;

    // calcu deg & build part of edges
    std::map<int64_t, int> deg;
    for (size_t i = 0; i < resultEdges.size(); i++) {
        deg[resultEdges[i].source]++;
        deg[resultEdges[i].target]--;

        pgr_costFlow_t edge;
        edge.edge_id = resultEdges[i].id;
        edge.reverse_capacity = -1;
        edge.reverse_cost = -1.0;
        edge.source = resultEdges[i].source;
        edge.target = resultEdges[i].target;
        edge.capacity = (std::numeric_limits<int32_t>::max)();
        edge.cost = resultEdges[i].cost;
        edges.push_back(edge);
    }

    // find superSource & superTarget
    superSource = superTarget = -1;
    int64_t iPointId = 1;
    while (superSource == -1 || superTarget == -1) {
        if (deg.find(iPointId) == deg.end()) {
            if (superSource == -1)
                superSource = iPointId;
            else
                superTarget = iPointId;
        }
        iPointId++;
    }
    sources.insert(superSource);
    targets.insert(superTarget);

    // build full edges
    std::map<int64_t, int>::iterator iter;
    totalDeg = 0;
    for (iter = deg.begin(); iter != deg.end(); iter++) {
        int64_t p = iter->first;
        int d = iter->second;
        if (d == 0)
            continue;
        if (d > 0)
            totalDeg += d;
        pgr_costFlow_t edge;
        edge.reverse_capacity = -1;
        edge.reverse_cost = -1.0;
        edge.cost = 0.0;
        edge.capacity = abs(d);
        edge.edge_id = 0;
        if (d > 0)
            edge.source = p, edge.target = superTarget;
        if (d < 0)
            edge.source = superSource, edge.target = p;
        edges.push_back(edge);
    }
    
    PgrCostFlowGraph graph(edges, sources, targets);
    flowGraph = graph;
}

std::vector<General_path_element_t>
PgrDirectedChPPGraph::GetPathEdges() {
    // catch new edges
    std::vector<pgr_flow_t> addedEdges = flowGraph.GetFlowEdges();
    for (auto &flow_t : addedEdges) {
        if (flow_t.source != superSource && flow_t.source != superTarget)
            if (flow_t.target != superSource && flow_t.target != superTarget) {
                pgr_edge_t newEdge;
                newEdge.id = flow_t.edge;
                newEdge.source = flow_t.source;
                newEdge.target = flow_t.target;
                newEdge.cost = flow_t.cost / static_cast<double>(flow_t.flow);
                newEdge.reverse_cost = -1.0;
                while (flow_t.flow--)
                    resultEdges.push_back(newEdge);
            }
    }

    BuildResultGraph();

    bool succ = EulerCircuitDFS(startPoint, resultGraph[VToVecid[startPoint]].second.end());
    if (!succ)
        resultPath.clear();
    else {
        General_path_element_t newElement;
        newElement.node = startPoint;
        newElement.edge = -1;
        newElement.cost = 0; 
	    if (resultPath.empty()) {
	        newElement.seq = 1;
	        newElement.agg_cost = 0.0;	
	    } else {
            newElement.seq = resultPath.back().seq + 1;
            newElement.agg_cost = resultPath.back().agg_cost + resultPath.back().cost;
	    }
        resultPath.push_back(newElement);
    }
    return resultPath;
}
    
// perform DFS approach to generate Euler circuit
// TODO(mg) find suitable API in BGL, maybe DfsVisitor will work.
// Implement DFS without BGL for now
bool
PgrDirectedChPPGraph::EulerCircuitDFS(int64_t p,
				      std::vector<size_t>::iterator edgeToFaIter) {
    for (std::vector<size_t>::iterator iter = resultGraph[VToVecid[p]].second.begin();
         iter != resultGraph[VToVecid[p]].second.end();
         ++iter) {
        if (!edgeVisited[*iter]) {
            if (edgeToFaIter != resultGraph[VToVecid[p]].second.end()) {
                pgr_edge_t edge_t = resultEdges[*edgeToFaIter];
    	    	General_path_element_t newElement;
    	    	newElement.node = edge_t.source;
    	    	newElement.edge = edge_t.id;
    	    	newElement.cost = edge_t.cost;
    	    	if (resultPath.empty()) {
                    newElement.seq = 1;
                    newElement.agg_cost = 0.0;
    	    	} else {
            	    newElement.seq = resultPath.back().seq + 1;
            	    newElement.agg_cost = resultPath.back().agg_cost + resultPath.back().cost;
    	    	}
                edgeVisited[*iter] = true;
    	        resultPath.push_back(newElement);
                EulerCircuitDFS(resultEdges[*iter].target, iter);
	        }
        }
    }
}

void
PgrDirectedChPPGraph::BuildResultGraph() {
    resultGraph.clear();
    VToVecid.clear();
    edgeVisited.clear();
    resultPath.resize(resultEdges.size());
    for (size_t i = 0; i < resultEdges.size(); i++) {
        pgr_edge_t edge_t = resultEdges[i];
        edgeVisited.push_back(false);
        if (VToVecid.find(edge_t.source) == VToVecid.end()) {
            VToVecid.insert(std::pair<int64_t, size_t>(edge_t.source, resultGraph.size()));
            resultGraph.resize(resultGraph.size() + 1);
        }
        size_t vid = VToVecid[edge_t.source];
        resultGraph[vid].second.push_back(i);
    }
}


}  // namespace graph
}  // namespace pgrouting

#endif  // INCLUDE_CHPP_PGR_DIRECTEDCHPP_HPP_
