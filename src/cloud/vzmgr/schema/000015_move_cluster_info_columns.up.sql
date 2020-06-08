ALTER TABLE vizier_cluster
ADD COLUMN cluster_name varchar(1000),
ADD COLUMN cluster_version varchar(1000),
ADD COLUMN cluster_uid varchar(1000);

ALTER TABLE vizier_cluster
ADD UNIQUE (org_id, cluster_name);

UPDATE vizier_cluster
SET cluster_name = vizier_cluster_info.cluster_name, cluster_version = vizier_cluster_info.cluster_version,
cluster_uid = vizier_cluster_info.cluster_uid
FROM vizier_cluster_info WHERE vizier_cluster.id = vizier_cluster_info.vizier_cluster_id;
