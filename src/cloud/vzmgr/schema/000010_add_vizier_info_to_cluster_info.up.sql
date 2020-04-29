ALTER TABLE vizier_cluster_info
ADD COLUMN vizier_version varchar(1000),
ADD COLUMN cluster_name varchar(1000),
ADD COLUMN cluster_version varchar(1000),
ADD COLUMN cluster_uid varchar(1000);
