package controllers

import (
	"fmt"
	"strings"

	"github.com/emicklei/dot"
	uuid "github.com/satori/go.uuid"
	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	"pixielabs.ai/pixielabs/src/carnot/planpb"
)

func styleGraphNodeForPlan(p *planpb.PlanNode, n dot.Node) dot.Node {
	switch op := p.Op.OpType; op {
	case planpb.GRPC_SINK_OPERATOR:
		n.Attr("color", "yellow")
		n.Attr("shape", "rect")
	case planpb.GRPC_SOURCE_OPERATOR:
		n.Attr("color", "darkorange")
		n.Attr("shape", "rect")
	case planpb.MEMORY_SINK_OPERATOR:
		n.Attr("color", "red")
		n.Attr("shape", "rect")
	case planpb.MEMORY_SOURCE_OPERATOR:
		n.Attr("color", "blue")
		n.Attr("shape", "rect")
	}

	nodeLabel := fmt.Sprintf("%s[%d]", strings.ToLower(p.Op.OpType.String()), p.Id)
	return n.Label(nodeLabel)
}

func graphNodeName(agentIDStr string, nodeID uint64) string {
	return fmt.Sprintf("%s_%d", agentIDStr, nodeID)
}

func getQueryPlanAsDotString(distributedPlan *distributedpb.DistributedPlan, planMap map[uuid.UUID]*planpb.Plan) (string, error) {
	g := dot.NewGraph(dot.Directed)

	// Node map keeps a map of strings to the graphviz nodes.
	nodeMap := make(map[string]dot.Node, 0)
	for agentID, plan := range planMap {
		agentIDStr := agentID.String()
		subGraphName := fmt.Sprintf("agent::%s", agentIDStr)

		s := g.Subgraph(subGraphName, dot.ClusterOption{})
		s.Value(subGraphName)
		s.Attr("color", "lightgrey")

		if len(plan.Nodes) == 0 {
			continue
		}
		queryFragment := plan.Nodes[0]
		dag := queryFragment.Dag

		for _, node := range queryFragment.Nodes {
			nodeName := graphNodeName(agentIDStr, node.Id)
			nodeMap[nodeName] = styleGraphNodeForPlan(node, s.Node(nodeName))
		}

		for _, node := range dag.Nodes {
			if len(node.SortedChildren) == 0 {
				continue
			}
			for _, child := range node.SortedChildren {
				nodeName := graphNodeName(agentIDStr, node.Id)
				childName := graphNodeName(agentIDStr, child)
				s.Edge(nodeMap[nodeName], nodeMap[childName])
			}
		}
	}

	// Stitch up distributed plan nodes.
	// This done by finding all the GRPC sink operators and looking up the destination ID.
	// The destination ID refers to a node in another agents subgraph. The other agent subgraph is found
	// by looking at the distributed plan dag.

	// We need an inverted index to lookup the agentID by the distributed dag ID.
	DAGIDToAgentIDMap := make(map[uint64]string)
	for agentID, dagID := range distributedPlan.QbAddressToDagId {
		DAGIDToAgentIDMap[dagID] = agentID
	}

	distChildrenList := make(map[uint64][]uint64)
	for _, node := range distributedPlan.Dag.Nodes {
		distChildrenList[node.Id] = node.SortedChildren
	}

	for agentID, plan := range planMap {
		agentIDStr := agentID.String()
		if len(plan.Nodes) == 0 {
			continue
		}
		queryFragment := plan.Nodes[0]
		for _, node := range queryFragment.Nodes {
			if node.Op.OpType == planpb.GRPC_SINK_OPERATOR {
				fromNodeName := graphNodeName(agentIDStr, node.Id)
				dest := node.Op.GetGRPCSinkOp().DestinationId
				// Look up the children of the current agent and find all the valid destinations.
				dagID := distributedPlan.QbAddressToDagId[agentID.String()]
				for _, childID := range distChildrenList[dagID] {
					nodeName := graphNodeName(DAGIDToAgentIDMap[childID], dest)
					if gNode, ok := nodeMap[nodeName]; ok {
						g.Edge(nodeMap[fromNodeName], gNode)
					}
				}
			}
		}
	}

	return g.String(), nil
}
