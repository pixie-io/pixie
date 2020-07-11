package controller

import (
	"database/sql/driver"
	"encoding/json"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
)

// PodStatuses Type to use in sqlx for the map of pod statuses.
type PodStatuses map[string]*cvmsgspb.PodStatus

// Value Returns a golang database/sql driver value for PodStatuses.
func (p PodStatuses) Value() (driver.Value, error) {
	res, err := json.Marshal(p)
	if err != nil {
		return res, err
	}
	return driver.Value(res), err
}

// Scan Scans the sqlx database type ([]bytes) into the PodStatuses type.
func (p *PodStatuses) Scan(src interface{}) error {
	var jsonText []byte
	switch src.(type) {
	case []byte:
		jsonText = src.([]byte)
	default:
		return status.Error(codes.Internal, "could not unmarshal control plane pod statuses")
	}

	err := json.Unmarshal(jsonText, p)
	if err != nil {
		return status.Error(codes.Internal, "could not unmarshal control plane pod statuses")
	}
	return nil
}
