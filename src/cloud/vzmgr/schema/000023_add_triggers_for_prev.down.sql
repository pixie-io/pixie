DROP TRIGGER IF EXISTS update_vizier_cluster_info_prev_state
  ON vizier_cluster_info;

ALTER TABLE vizier_cluster_info DROP COLUMN prev_status;
ALTER TABLE vizier_cluster_info DROP COLUMN prev_status_time;
ALTER TABLE vizier_cluster_info DROP COLUMN prev_status_message;
