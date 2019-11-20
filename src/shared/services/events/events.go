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
	// SiteCreated is the evnet for a new site.
	SiteCreated = "Site Created"
)
