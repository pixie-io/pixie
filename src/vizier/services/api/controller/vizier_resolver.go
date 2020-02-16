package controller

import (
	"context"
	"math"
	"sort"
	"time"

	"github.com/graph-gophers/graphql-go"

	"github.com/golang/protobuf/jsonpb"
	uuid "github.com/satori/go.uuid"

	"pixielabs.ai/pixielabs/src/carnot/planner/compilerpb"
	qrpb "pixielabs.ai/pixielabs/src/carnot/queryresultspb"

	logicalplanner "pixielabs.ai/pixielabs/src/carnot/planner"
	plannerpb "pixielabs.ai/pixielabs/src/carnot/planner/plannerpb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	schemapb "pixielabs.ai/pixielabs/src/table_store/proto"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/vizier/services/api/apienv"
	qbpb "pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
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

func makeLineColError(errorPB *compilerpb.LineColError) *LineColError {
	if errorPB == nil {
		return nil
	}
	newError := &LineColError{
		Line: int32(errorPB.Line),
		Col:  int32(errorPB.Column),
		Msg:  errorPB.Message,
	}
	return newError

}

func makeErrorFromStatus(status *statuspb.Status) (*QueryError, error) {
	queryError := new(QueryError)
	compilerError := new(CompilerError)
	compilerError.Msg = &status.Msg
	queryError.CompilerError = compilerError
	if !logicalplanner.HasContext(status) {
		return queryError, nil
	}

	// Convert the LineCol
	compilerErrorGroupPB := new(compilerpb.CompilerErrorGroup)
	logicalplanner.GetCompilerErrorContext(status, compilerErrorGroupPB)

	lineColErrsArr := make([]*LineColError, len(compilerErrorGroupPB.Errors))
	compilerError.LineColErrors = &lineColErrsArr
	for i, errorPB := range compilerErrorGroupPB.Errors {
		(*compilerError.LineColErrors)[i] = makeLineColError(errorPB.GetLineColError())
	}

	return queryError, nil
}

// ExecuteQuery executes a query on vizier.
func (q *QueryResolver) ExecuteQuery(ctx context.Context, args *executeQueryArgs) (*QueryResultResolver, error) {
	client := q.Env.QueryBrokerClient()
	req := plannerpb.QueryRequest{QueryStr: *args.QueryStr}

	resp, err := client.ExecuteQuery(ctx, &req)
	if err != nil {
		return nil, err
	}
	returnedQueryID := resp.QueryID

	u, err := utils.UUIDFromProto(returnedQueryID)
	if err != nil {
		return nil, err
	}

	// If there's a compiler error, we need to do something about it.
	if resp.Status != nil && resp.Status.ErrCode != statuspb.OK {
		queryError, err := makeErrorFromStatus(resp.Status)
		if err != nil {
			return nil, err
		}
		errorResult := &QueryResultResolver{u, &qrpb.QueryResult{}, queryError}
		return errorResult, nil
	}

	return &QueryResultResolver{u, resp.QueryResult, &QueryError{}}, nil
}

/**
 * Resolvers for Vizier Metadata.
 */

// VizierInfoResolver is the resolver responsible for getting metadata from vizier.
type VizierInfoResolver struct {
	SessionCtx *authcontext.AuthContext
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
		return resolvers[i].Metadata.Agent.Info.HostInfo.Hostname < resolvers[j].Metadata.Agent.Info.HostInfo.Hostname
	})

	return &resolvers, nil
}

// AgentStatusResolver resolves agent status information.
type AgentStatusResolver struct {
	Metadata *qbpb.AgentMetadata
}

// LastHeartBeatMs returns the time since last heartbeat.
func (a *AgentStatusResolver) LastHeartBeatMs() float64 {
	hb := a.Metadata.Status.NSSinceLastHeartbeat
	return float64(hb / 1e6)
}

// UptimeS returns the time in seconds that the agent has been alive.
func (a *AgentStatusResolver) UptimeS() float64 {
	// TOOD(zasgar): Consider having the metadata service return relative time.
	// Remove negatives.
	hb := math.Max(float64(time.Now().UnixNano()-a.Metadata.Agent.CreateTimeNS), 0.0)
	return hb / 1.0e9
}

// State returns the state of the given agent.
func (a *AgentStatusResolver) State() string {
	return a.Metadata.Status.State.String()
}

// Info returns agent information.
func (a *AgentStatusResolver) Info() *AgentInfoResolver {
	return &AgentInfoResolver{a.Metadata.Agent.Info}
}

// AgentInfoResolver resolves agent information.
type AgentInfoResolver struct {
	Info *agentpb.AgentInfo
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
	HostInfo *agentpb.HostInfo
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
	Error   *QueryError
}

// QueryError contains all possible errors that aren't system breaking in the query.
type QueryError struct {
	CompilerError *CompilerError
}

// CompilerError contains the accumulated information about compiler errors.
type CompilerError struct {
	Msg           *string
	LineColErrors *[]*LineColError
}

// LineColError is a line column error.
type LineColError struct {
	Line int32
	Col  int32
	Msg  string
}

// ID returns the ID of the query (as string).
func (q *QueryResultResolver) ID() graphql.ID {
	return graphql.ID(q.QueryID.String())
}

// Table returns a resolver for a table. Selects first returned table.
func (q *QueryResultResolver) Table() *[]*QueryResultTableResolver {
	tables := make([]*QueryResultTableResolver, len(q.Result.Tables))

	if len(q.Result.Tables) == 0 {
		return nil
	}

	for i, table := range q.Result.Tables {
		selectedTable := table
		names := []string{}
		types := []string{}
		for _, col := range selectedTable.Relation.Columns {
			names = append(names, col.ColumnName)
			types = append(types, col.ColumnType.String())
		}
		relation := QueryResultTableRelationResolver{&names, &types}
		tables[i] = &QueryResultTableResolver{relation, selectedTable}
	}

	return &tables
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

// Name returns the resolver for the table name.
func (q *QueryResultTableResolver) Name() *string {
	return &q.TableData.Name
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
