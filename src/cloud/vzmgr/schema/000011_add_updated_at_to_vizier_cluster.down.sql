ALTER TABLE vizier_cluster
DROP COLUMN updated_at;

DROP TRIGGER IF EXISTS update_vizier_cluster_updated_at ON vizier_cluster;
