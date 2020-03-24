-- (org_id, project_name) is now the owner of this cluster, not org_id.
ALTER TABLE vizier_cluster ADD COLUMN project_name varchar(50) NOT NULL DEFAULT 'default';
