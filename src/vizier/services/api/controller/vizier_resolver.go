package controller

import (
	"context"
	"errors"
	"fmt"
	"math"
	"sort"
	"time"

	"github.com/golang/protobuf/jsonpb"
	"github.com/graph-gophers/graphql-go"
	uuid "github.com/satori/go.uuid"
	qrpb "pixielabs.ai/pixielabs/src/carnot/queryresultspb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	"pixielabs.ai/pixielabs/src/services/common/sessioncontext"
	schemapb "pixielabs.ai/pixielabs/src/table_store/proto"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/vizier/services/api/apienv"
	qbpb "pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
)

// GQLInt64Type stores 64-bit numbers as two int32, since JS does not support
// 64-bit integers.
type GQLInt64Type struct {
	L int32
	H int32
}

type executeQueryArgs struct {
	QueryStr *string
}

// ExecuteQuery executes a query on vizier.
func (q *QueryResolver) ExecuteQuery(ctx context.Context, args *executeQueryArgs) (*QueryResultResolver, error) {
	client := q.Env.QueryBrokerClient()
	req := qbpb.QueryRequest{QueryStr: *args.QueryStr}

	resp, err := client.ExecuteQuery(ctx, &req)
	if err != nil {
		return nil, err
	}

	// Find the response with the most data.
	var maxQR *qrpb.QueryResult
	// TODO(zasgar/michelle): This is a placeholder until we add results merging.
	// This simply selects the largest resulting table.
	maxResultsSize := int(-1)
	for _, r := range resp.Responses {
		if r.Response.Status != nil && r.Response.Status.ErrCode != statuspb.OK {
			return nil, fmt.Errorf("failed to execute: %s", r.Response.Status.Msg)
		}
		if maxResultsSize < r.Response.QueryResult.Tables[0].Size() {
			maxQR = r.Response.QueryResult
			maxResultsSize = r.Response.QueryResult.Tables[0].Size()
		}
	}

	if maxResultsSize < 0 {
		return nil, errors.New("no results were returned")
	}

	returnedQueryID := resp.Responses[0].Response.QueryID
	u, err := utils.UUIDFromProto(returnedQueryID)
	if err != nil {
		return nil, err
	}
	// Always return response from first agent.
	return &QueryResultResolver{u, maxQR}, nil
}

/**
 * Resolvers for Vizier Metadata.
 */

// VizierInfoResolver is the resolver responsible for getting metadata from vizier.
type VizierInfoResolver struct {
	SessionCtx *sessioncontext.SessionContext
	Env        apienv.APIEnv
}

// Agents gets information about agents connected to vizier.
func (v *VizierInfoResolver) Agents(ctx context.Context) (*[]*AgentStatusResolver, error) {
	client := v.Env.QueryBrokerClient()
	req := qbpb.AgentInfoRequest{}
	resp, err := client.GetAgentInfo(ctx, &req)
	if err != nil {
		return nil, err
	}
	resolvers := []*AgentStatusResolver{}
	for _, info := range resp.Info {
		resolvers = append(resolvers, &AgentStatusResolver{info})
	}
	// Sort agents by hostname.
	sort.Slice(resolvers, func(i, j int) bool {
		return resolvers[i].Status.Info.HostInfo.Hostname < resolvers[j].Status.Info.HostInfo.Hostname
	})

	return &resolvers, nil
}

// AgentStatusResolver resolves agent status information.
type AgentStatusResolver struct {
	Status *qbpb.AgentStatus
}

// LastHeartBeatMs returns the time since last heartbeat.
func (a *AgentStatusResolver) LastHeartBeatMs() float64 {
	hb := a.Status.LastHeartbeatNs
	return float64(hb / 1E6)
}

// UptimeS returns the time in seconds that the agent has been alive.
func (a *AgentStatusResolver) UptimeS() float64 {
	// TOOD(zasgar): Consider having the metadata service return relative time.
	// Remove negatives.
	hb := math.Max(float64(time.Now().UnixNano()-a.Status.CreateTimeNs), 0.0)
	return hb / 1.0E9
}

// State returns the state of the given agent.
func (a *AgentStatusResolver) State() string {
	return a.Status.State.String()
}

// Info returns agent information.
func (a *AgentStatusResolver) Info() *AgentInfoResolver {
	return &AgentInfoResolver{a.Status.Info}
}

// AgentInfoResolver resolves agent information.
type AgentInfoResolver struct {
	Info *qbpb.AgentInfo
}

// ID returns agent ID.
func (a *AgentInfoResolver) ID() graphql.ID {
	agentUUID, _ := uuid.FromString(string(a.Info.AgentID.Data))
	return graphql.ID(agentUUID.String())
}

// HostInfo gets host information for agent.
func (a *AgentInfoResolver) HostInfo() *HostInfoResolver {
	return &HostInfoResolver{a.Info.HostInfo}
}

// HostInfoResolver resolves agent host information.
type HostInfoResolver struct {
	HostInfo *qbpb.HostInfo
}

// Hostname returns the hostname of the machine where the agent is running.
func (h *HostInfoResolver) Hostname() *string {
	return &h.HostInfo.Hostname
}

/**
 * Resolver for query results.
 */

// QueryResultResolver resolves results of a query.
type QueryResultResolver struct {
	QueryID uuid.UUID
	Result  *qrpb.QueryResult
}

// ID returns the ID of the query (as string).
func (q *QueryResultResolver) ID() graphql.ID {
	return graphql.ID(q.QueryID.String())
}

// Table returns a resolver for a table. Selects first returned table.
func (q *QueryResultResolver) Table() *QueryResultTableResolver {
	// Always select first table.
	selectedTable := q.Result.Tables[0]
	names := []string{}
	types := []string{}
	for _, col := range selectedTable.Relation.Columns {
		names = append(names, col.ColumnName)
		types = append(types, col.ColumnType.String())
	}
	relation := QueryResultTableRelationResolver{&names, &types}
	return &QueryResultTableResolver{relation, selectedTable}
}

/**
 * Resolver which maps table into GQL type.
 */

// QueryResultTableResolver resolves table information for executed query.
type QueryResultTableResolver struct {
	relation  QueryResultTableRelationResolver
	TableData *schemapb.Table
}

// Data returns table data as a JSON marshalled proto.
func (q *QueryResultTableResolver) Data() (string, error) {
	m := jsonpb.Marshaler{}
	str, err := m.MarshalToString(q.TableData)
	if err != nil {
		return "", err
	}
	return str, nil
}

// Relation returns the resolver for table relations.
func (q *QueryResultTableResolver) Relation() *QueryResultTableRelationResolver {
	return &q.relation
}

/**
 * Resolver that maps a table relation into GQL types.
 */

// QueryResultTableRelationResolver resolves relation information for a table.
type QueryResultTableRelationResolver struct {
	colNames *[]string
	colTypes *[]string
}

// ColNames returns a list of column names.
func (r *QueryResultTableRelationResolver) ColNames() *[]string {
	return r.colNames
}

// ColTypes returns a list of columnTypes.
func (r *QueryResultTableRelationResolver) ColTypes() *[]string {
	return r.colTypes
}
