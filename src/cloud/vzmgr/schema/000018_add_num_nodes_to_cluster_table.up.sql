ALTER TABLE vizier_cluster_info
ADD COLUMN num_nodes INT NOT NULL DEFAULT 0;

ALTER TABLE vizier_cluster_info
ADD COLUMN num_instrumented_nodes INT NOT NULL DEFAULT 0;
