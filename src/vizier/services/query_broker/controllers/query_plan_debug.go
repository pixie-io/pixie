package controllers

import (
	"fmt"
	"strings"
	"time"

	"github.com/dustin/go-humanize"
	"github.com/emicklei/dot"
	uuid "github.com/satori/go.uuid"
	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	"pixielabs.ai/pixielabs/src/carnot/planpb"
	"pixielabs.ai/pixielabs/src/carnot/queryresultspb"
	"pixielabs.ai/pixielabs/src/utils"
)

func styleGraphNodeForPlan(p *planpb.PlanNode, n dot.Node, extraDetails string) dot.Node {
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

	// If extraDetails is the empty string, dot strips the extra newline.
	nodeLabel := fmt.Sprintf("%s[%d]\n%s", strings.ToLower(p.Op.OpType.String()), p.Id, extraDetails)
	return n.Label(nodeLabel)
}

func graphNodeName(agentIDStr string, nodeID uint64) string {
	return fmt.Sprintf("%s_%d", agentIDStr, nodeID)
}

func timeNSToString(timeNS int64) string {
	return (time.Nanosecond * time.Duration(timeNS)).String()
}

func nodeExecTiming(nodeID int64, execStats *map[int64]*queryresultspb.OperatorExecutionStats) string {
	stats, ok := (*execStats)[nodeID]
	if !ok {
		return ""
	}
	return fmt.Sprintf("self_time: %s\ntotal_time: %s\nbytes: %s\nrecords_processed: %d", timeNSToString(stats.SelfExecutionTimeNs), timeNSToString(stats.TotalExecutionTimeNs), humanize.Bytes(uint64(stats.BytesOutput)), stats.RecordsOutput)
}

func getQueryPlanAsDotString(distributedPlan *distributedpb.DistributedPlan, planMap map[uuid.UUID]*planpb.Plan, planExecStats *[]*queryresultspb.AgentExecutionStats) (string, error) {
	g := dot.NewGraph(dot.Directed)
	execDetails := make(map[uuid.UUID]*queryresultspb.AgentExecutionStats, 0)
	if planExecStats != nil {
		for _, execStat := range *planExecStats {
			agentID := utils.UUIDFromProtoOrNil(execStat.AgentID)
			execDetails[agentID] = execStat
		}
	}

	// Node map keeps a map of strings to the graphviz nodes.
	nodeMap := make(map[string]dot.Node, 0)
	for agentID, plan := range planMap {
		agentExecStats, hasExecStats := execDetails[agentID]
		operatorExecStatsMap := make(map[int64]*queryresultspb.OperatorExecutionStats, 0)

		agentIDStr := agentID.String()
		subGraphName := fmt.Sprintf("agent::%s", agentIDStr)

		if hasExecStats {
			for _, OperatorExecutionStats := range agentExecStats.OperatorExecutionStats {
				operatorExecStatsMap[OperatorExecutionStats.NodeId] = OperatorExecutionStats
			}
			subGraphName += fmt.Sprintf("\n%s", timeNSToString(agentExecStats.ExecutionTimeNs))
		}

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
			extraDetails := nodeExecTiming(int64(node.Id), &operatorExecStatsMap)
			nodeMap[nodeName] = styleGraphNodeForPlan(node, s.Node(nodeName), extraDetails)
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
