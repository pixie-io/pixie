package vzshard

import (
	"fmt"

	uuid "github.com/satori/go.uuid"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
)

// SetupFlags install the flag handlers for vizier shards.
func SetupFlags() {
	pflag.Int("vizier_shard_min", 0, "The min vizier shard for this program (inclusive)")
	pflag.Int("vizier_shard_max", 255, "The max vizier shard for this program (inclusive)")
}

func minShard() int {
	return viper.GetInt("vizier_shard_min")
}

func maxShard() int {
	return viper.GetInt("vizier_shard_max")
}

func shardIntToHex(i int) string {
	return fmt.Sprintf("%02x", i)
}

// ShardMin returns the min hex value of the vizier shard.
func ShardMin() string {
	return shardIntToHex(minShard())
}

// ShardMax returns the max hex value of the vizier shard.
func ShardMax() string {
	return shardIntToHex(maxShard())
}

// GenerateShardRange shard range produces the hex values 00-ff for the shards as configured.
func GenerateShardRange() chan string {
	ch := make(chan string)
	go func() {
		defer close(ch)
		for i := minShard(); i <= maxShard(); i++ {
			ch <- shardIntToHex(i)
		}
	}()
	return ch
}

// VizierIDToShard provides the shardID for a given vizierID.
func VizierIDToShard(vizierID uuid.UUID) string {
	stringID := vizierID.String()
	return stringID[len(stringID)-2:]
}
