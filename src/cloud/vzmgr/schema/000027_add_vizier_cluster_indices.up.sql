CREATE INDEX idx_vizier_cluster_cluster_uid ON vizier_cluster(cluster_uid);
CREATE INDEX idx_vizier_cluster_cluster_name ON vizier_cluster(cluster_name);
CREATE INDEX idx_vizier_cluster_org_id ON vizier_cluster(org_id);

CREATE INDEX idx_vizier_cluster_info_status ON vizier_cluster_info(status);
CREATE INDEX idx_vizier_cluster_info_last_heartbeat ON vizier_cluster_info(last_heartbeat);
