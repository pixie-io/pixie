ALTER TABLE vizier_cluster_info
  ADD COLUMN previous_vizier_status vizier_status;

ALTER TABLE vizier_cluster_info
  ADD COLUMN previous_vizier_status_time TIMESTAMP;
