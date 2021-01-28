package pxapi

// ClientOption configures options on the client.
type ClientOption func(client *Client)

// WithCloudAddr is the option to specify cloud address to use.
func WithCloudAddr(cloudAddr string) ClientOption {
	return func(c *Client) {
		c.cloudAddr = cloudAddr
	}
}

// WithBearerAuth is the option to specify bearer auth to use.
func WithBearerAuth(auth string) ClientOption {
	return func(c *Client) {
		c.bearerAuth = auth
	}
}

// WithAPIKey is the option to specify the API key to use.
func WithAPIKey(auth string) ClientOption {
	return func(c *Client) {
		c.apiKey = auth
	}
}
