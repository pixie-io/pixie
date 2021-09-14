ALTER TABLE vizier_cluster_info
DROP COLUMN cluster_name,
DROP COLUMN cluster_uid;

ALTER TABLE vizier_cluster
DROP COLUMN cluster_version;
