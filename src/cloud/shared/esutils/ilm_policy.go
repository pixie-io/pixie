package esutils

import (
	"context"
	"encoding/json"
	"fmt"

	"github.com/olivere/elastic/v7"
	log "github.com/sirupsen/logrus"
)

const (
	// DefaultMaxIndexSize is the default size to allow the index to grow to before rollover.
	DefaultMaxIndexSize = "50gb"
	// DefaultTimeBeforeDelete is the default amount of time after rollover, to wait before deleting an index.
	DefaultTimeBeforeDelete = "0d"
)

func strPtr(s string) *string {
	return &s
}

type esILMPolicy struct {
	Policy map[string]interface{} `json:"policy"`
}

// ILMPolicy manages the creation/updating of an elastic index lifecycle management policy.
type ILMPolicy struct {
	es                  *elastic.Client
	policyName          string
	policy              *esILMPolicy
	errorDuringAssembly error
}

// NewILMPolicy create a new ILMPolicy with the given name and default policy actions.
func NewILMPolicy(es *elastic.Client, policyName string) *ILMPolicy {
	p := &ILMPolicy{
		es:         es,
		policyName: policyName,
		policy: &esILMPolicy{
			Policy: make(map[string]interface{}),
		},
	}
	return p.Rollover(strPtr(DefaultMaxIndexSize), nil, nil).DeleteAfter(DefaultTimeBeforeDelete)
}

func (p *ILMPolicy) mapForPhase(phaseName string) map[string]interface{} {
	if p.policy.Policy["phases"] == nil {
		p.policy.Policy["phases"] = make(map[string]interface{})
	}
	phases := p.policy.Policy["phases"].(map[string]interface{})

	if phases[phaseName] == nil {
		phases[phaseName] = make(map[string]interface{})
	}
	return phases[phaseName].(map[string]interface{})
}

// Rollover adds a rollover action to the policy's hot phase.
// This action causes the indices affected by the policy to rollover to a new index when the conditions
// specified by maxSize, maxDocs, and maxAge are met.
// Leaving a parameter as nil will cause that condition to be ignored, eg. Rollover(strPtr("10gb"), nil, nil)
// will only add a max size condition and not any of the others.
func (p *ILMPolicy) Rollover(maxSize *string, maxDocs *int, maxAge *string) *ILMPolicy {
	hot := p.mapForPhase("hot")
	hot["min_age"] = "0ms"

	if hot["actions"] == nil {
		hot["actions"] = make(map[string]interface{})
	}
	actions := hot["actions"].(map[string]interface{})

	if actions["rollover"] == nil {
		actions["rollover"] = make(map[string]interface{})
	}
	rollover := actions["rollover"].(map[string]interface{})

	if maxSize != nil {
		rollover["max_size"] = *maxSize
	}
	if maxDocs != nil {
		rollover["max_docs"] = *maxDocs
	}
	if maxAge != nil {
		rollover["max_age"] = *maxAge
	}
	return p
}

// DeleteAfter adds a delete action to the policy's delete phase.
// This actions causes rolled over indices to be deleted after `timeBeforeDelete` time has passed.
func (p *ILMPolicy) DeleteAfter(timeBeforeDelete string) *ILMPolicy {
	delete := p.mapForPhase("delete")
	delete["min_age"] = timeBeforeDelete

	if delete["actions"] == nil {
		delete["actions"] = make(map[string]interface{})
	}
	actions := delete["actions"].(map[string]interface{})
	actions["delete"] = make(map[string]interface{})

	return p
}

// FromJSONString populates the policy from a marshalled json string.
// Note that this can be used in conjunction with Rollover and Delete.
func (p *ILMPolicy) FromJSONString(policyJSONStr string) *ILMPolicy {
	if err := json.Unmarshal([]byte(policyJSONStr), &p.policy); err != nil {
		p.errorDuringAssembly = err
	}
	return p
}

func (p *ILMPolicy) String() string {
	policyStr, _ := json.MarshalIndent(p.policy, "", "  ")
	return string(policyStr)
}

// Migrate creates and/or updates the policy in elastic.
func (p *ILMPolicy) Migrate(ctx context.Context) error {
	// Creation and update have the same endpoint, so no need to check for existence.
	if err := p.validate(); err != nil {
		return err
	}
	return p.upsert(ctx)
}

func (p *ILMPolicy) validate() error {
	if p.errorDuringAssembly != nil {
		return p.errorDuringAssembly
	}
	return nil
}

func (p *ILMPolicy) upsert(ctx context.Context) error {
	policyBody, err := json.Marshal(p.policy)
	if err != nil {
		return err
	}
	resp, err := p.es.XPackIlmPutLifecycle().Policy(p.policyName).BodyString(string(policyBody)).Do(ctx)
	if err != nil {
		log.WithError(err).WithField("cause", err.(*elastic.Error).Details.CausedBy).
			Error("failed to update elastic ILM policy.")
		return err
	}
	if !resp.Acknowledged {
		return fmt.Errorf("elastic failed to create policy '%s'", p.policyName)
	}
	return nil
}
