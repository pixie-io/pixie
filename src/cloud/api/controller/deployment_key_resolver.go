package controller

import (
	"context"
	"sort"

	"github.com/graph-gophers/graphql-go"
	uuid "github.com/satori/go.uuid"

	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/utils"
)

// NanosPerSecond is the number of nanoseconds per second.
const NanosPerSecond int64 = 1000 * 1000 * 1000

// DeploymentKeyResolver is the resolver responsible for cluster info.
type DeploymentKeyResolver struct {
	id          uuid.UUID
	key         string
	createdAtNs int64
	desc        string
}

// ID returns deployment key ID.
func (d *DeploymentKeyResolver) ID() graphql.ID {
	return graphql.ID(d.id.String())
}

// Key returns the deployment key value.
func (d *DeploymentKeyResolver) Key() string {
	return d.key
}

// CreatedAtMs returns the time at which the deployment key was created.
func (d *DeploymentKeyResolver) CreatedAtMs() float64 {
	return float64(d.createdAtNs) / 1e6
}

// Desc returns the description of the key.
func (d *DeploymentKeyResolver) Desc() string {
	return d.desc
}

// CreateDeploymentKey creates a new deployment key.
func (q *QueryResolver) CreateDeploymentKey(ctx context.Context) (*DeploymentKeyResolver, error) {
	grpcAPI := q.Env.VizierDeployKeyMgr
	res, err := grpcAPI.Create(ctx, &cloudapipb.CreateDeploymentKeyRequest{})
	if err != nil {
		return nil, err
	}
	return deploymentKeyToResolver(res)
}

func deploymentKeyToResolver(key *cloudapipb.DeploymentKey) (*DeploymentKeyResolver, error) {
	keyID, err := utils.UUIDFromProto(key.ID)
	if err != nil {
		return nil, err
	}

	return &DeploymentKeyResolver{
		id:          keyID,
		key:         key.Key,
		createdAtNs: key.CreatedAt.Seconds*NanosPerSecond + int64(key.CreatedAt.Nanos),
		desc:        key.Desc,
	}, nil
}

// DeploymentKeys lists all of the deployment keys.
func (q *QueryResolver) DeploymentKeys(ctx context.Context) ([]*DeploymentKeyResolver, error) {
	grpcAPI := q.Env.VizierDeployKeyMgr
	res, err := grpcAPI.List(ctx, &cloudapipb.ListDeploymentKeyRequest{})
	if err != nil {
		return nil, err
	}
	var keys []*DeploymentKeyResolver
	for _, key := range res.Keys {
		resolvedKey, err := deploymentKeyToResolver(key)
		if err != nil {
			return nil, err
		}
		keys = append(keys, resolvedKey)
	}
	// Sort by descending time
	sort.Slice(keys, func(i, j int) bool { return keys[i].createdAtNs > keys[j].createdAtNs })
	return keys, nil
}

type getOrDeleteDeployKeyArgs struct {
	ID graphql.ID
}

// DeploymentKey gets a specific deployment key.
func (q *QueryResolver) DeploymentKey(ctx context.Context, args *getOrDeleteDeployKeyArgs) (*DeploymentKeyResolver, error) {
	grpcAPI := q.Env.VizierDeployKeyMgr
	res, err := grpcAPI.Get(ctx, &cloudapipb.GetDeploymentKeyRequest{
		ID: utils.ProtoFromUUIDStrOrNil(string(args.ID)),
	})
	if err != nil {
		return nil, err
	}
	return deploymentKeyToResolver(res.Key)
}

// DeleteDeploymentKey deletes a specific deployment key.
func (q *QueryResolver) DeleteDeploymentKey(ctx context.Context, args *getOrDeleteDeployKeyArgs) (bool, error) {
	grpcAPI := q.Env.VizierDeployKeyMgr
	_, err := grpcAPI.Delete(ctx, utils.ProtoFromUUIDStrOrNil(string(args.ID)))
	if err != nil {
		return false, err
	}
	return true, nil
}
