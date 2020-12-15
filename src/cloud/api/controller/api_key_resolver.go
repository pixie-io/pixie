package controller

import (
	"context"
	"errors"

	"github.com/graph-gophers/graphql-go"
	uuid "github.com/satori/go.uuid"
)

// APIKeyResolver is the resolver responsible for API keys.
type APIKeyResolver struct {
	id          uuid.UUID
	key         string
	createdAtNs int64
	desc        string
}

// ID returns API key ID.
func (d *APIKeyResolver) ID() graphql.ID {
	return graphql.ID(d.id.String())
}

// Key returns the API key value.
func (d *APIKeyResolver) Key() string {
	return d.key
}

// CreatedAtMs returns the time at which the API key was created.
func (d *APIKeyResolver) CreatedAtMs() float64 {
	return float64(d.createdAtNs) / 1e6
}

// Desc returns the description of the key.
func (d *APIKeyResolver) Desc() string {
	return d.desc
}

// CreateAPIKey creates a new API key.
func (q *QueryResolver) CreateAPIKey(ctx context.Context) (*APIKeyResolver, error) {
	return nil, errors.New("Not yet implemented")
}

// APIKeys lists all of the API keys.
func (q *QueryResolver) APIKeys(ctx context.Context) ([]*APIKeyResolver, error) {
	return nil, errors.New("Not yet implemented")
}

type getOrDeleteAPIKeyArgs struct {
	ID graphql.ID
}

// APIKey gets a specific API key.
func (q *QueryResolver) APIKey(ctx context.Context, args *getOrDeleteAPIKeyArgs) (*APIKeyResolver, error) {
	return nil, errors.New("Not yet implemented")
}

// DeleteAPIKey deletes a specific API key.
func (q *QueryResolver) DeleteAPIKey(ctx context.Context, args *getOrDeleteAPIKeyArgs) (bool, error) {
	return false, errors.New("Not yet implemented")
}
