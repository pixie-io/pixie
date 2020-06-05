-- We need to convert this to unique org cluster name after we move this to the other table.
ALTER TABLE vizier_cluster_info ADD CONSTRAINT uniq_cluster_name UNIQUE (cluster_name);
