package pg

import (
	"testing"

	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
)

func TestDefaultDBURI(t *testing.T) {
	viper.Set("postgres_port", 5000)
	viper.Set("postgres_hostname", "postgres-host")
	viper.Set("postgres_db", "thedb")
	viper.Set("postgres_username", "user")
	viper.Set("postgres_password", "pass")

	t.Run("With SSL", func(t *testing.T) {
		viper.Set("postgres_ssl", true)
		assert.Equal(t, DefaultDBURI(), "postgres://user:pass@postgres-host:5000/thedb?sslmode=require")
	})

	t.Run("Without SSL", func(t *testing.T) {
		viper.Set("postgres_ssl", false)
		assert.Equal(t, DefaultDBURI(), "postgres://user:pass@postgres-host:5000/thedb?sslmode=disable")
	})
}
