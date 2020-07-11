ALTER TABLE vizier_cluster_info
ADD COLUMN control_plane_pod_statuses json NOT NULL DEFAULT '{}';
