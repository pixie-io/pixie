-- Shards are the lower 8-bits of the UUID string so we create an index on that.
CREATE INDEX vizier_cluster_shard_index ON vizier_cluster (substring(id::text, 35));
