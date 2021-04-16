package controllers

import (
	"context"

	"px.dev/pixie/src/cloud/shared/idprovider"
)

func transformKratosUserInfoToUserInfo(kratosUser *idprovider.KratosUserInfo) (*UserInfo, error) {
	// If user does not exist in Auth0, then create a new user if specified.
	u := &UserInfo{
		Email:    kratosUser.Email,
		PLUserID: kratosUser.PLUserID,
		PLOrgID:  kratosUser.PLOrgID,
	}
	return u, nil
}

// HydraKratosUserClient exposes user management for hydra and kratos.
type HydraKratosUserClient interface {
	GetUserIDFromToken(ctx context.Context, token string) (string, error)
	GetUserInfo(ctx context.Context, userID string) (*idprovider.KratosUserInfo, error)
	UpdateUserInfo(ctx context.Context, userID string, kratosInfo *idprovider.KratosUserInfo) (*idprovider.KratosUserInfo, error)
}

// HydraKratosConnector implements the AuthProvider interface for Hydra + Kratos.
type HydraKratosConnector struct {
	client HydraKratosUserClient
}

// NewHydraKratosConnector provides an implementation of an HydraKratosConnector.
func NewHydraKratosConnector() (*HydraKratosConnector, error) {
	client, err := idprovider.NewHydraKratosClient()
	if err != nil {
		return nil, err
	}
	return &HydraKratosConnector{client}, nil
}

// GetUserIDFromToken returns the UserID for the particular token.
func (a *HydraKratosConnector) GetUserIDFromToken(token string) (string, error) {
	return a.client.GetUserIDFromToken(context.Background(), token)
}

// GetUserInfo returns the UserInfo for this userID.
func (a *HydraKratosConnector) GetUserInfo(userID string) (*UserInfo, error) {
	kratosInfo, err := a.client.GetUserInfo(context.Background(), userID)
	if err != nil {
		return nil, err
	}

	return transformKratosUserInfoToUserInfo(kratosInfo)
}

// SetPLMetadata sets the pixielabs related metadata in Kratos.
func (a *HydraKratosConnector) SetPLMetadata(userID, plOrgID, plUserID string) error {
	// Grab the original UserInfo.
	kratosInfo, err := a.client.GetUserInfo(context.Background(), userID)
	if err != nil {
		return err
	}
	kratosInfo.PLOrgID = plOrgID
	kratosInfo.PLUserID = plUserID
	_, err = a.client.UpdateUserInfo(context.Background(), userID, kratosInfo)
	return err
}
