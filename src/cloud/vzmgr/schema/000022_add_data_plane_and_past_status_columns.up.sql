ALTER TABLE vizier_cluster_info
ADD COLUMN unhealthy_data_plane_pod_statuses json NOT NULL DEFAULT '{}';

ALTER TABLE vizier_cluster_info
ADD COLUMN previous_vizier_status vizier_status;

ALTER TABLE vizier_cluster_info
ADD COLUMN previous_vizier_status_time TIMESTAMP;
