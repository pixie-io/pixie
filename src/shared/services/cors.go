package services

import (
	"github.com/gorilla/handlers"
)

// DefaultCORSConfig has the default config setup for CORS.
func DefaultCORSConfig() []handlers.CORSOption {
	return []handlers.CORSOption{
		handlers.AllowedMethods([]string{"POST", "OPTIONS"}),
		handlers.AllowedHeaders([]string{"Content-Type", "Origin", "Accept", "token", "authorization"}),
		// TODO(michellenguyen/zasgar, PP-2581): Make this more restrictive.
		handlers.AllowedOrigins([]string{"*"}),
	}
}
