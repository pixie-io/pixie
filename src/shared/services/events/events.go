package events

// This file tracks all the events the backend produces to
// reduce the chance of a typo messing up the analytics.

const (
	// UserLoggedIn is the login event.
	UserLoggedIn = "Logged In"
	// UserSignedUp is the signup event.
	UserSignedUp = "Signed Up"
	// OrgCreated is the event for a new Org.
	OrgCreated = "Org Created"
	// SiteCreated is the event for a new site.
	SiteCreated = "Site Created"
	// ClusterStatusChange is an event for when a Vizier cluster's status changes.
	ClusterStatusChange = "Cluster Status Change"
	// VizierHeartbeat is an event for when Vizier sends a heartbeat to the cloud.
	VizierHeartbeat = "Vizier Heartbeat"
	// APIRequest is an event for when a request is made to the Pixie API using an API token.
	APIRequest = "API Request"
)
