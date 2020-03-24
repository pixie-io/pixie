-- org_id alone is now the owner of the cluster again.
ALTER TABLE vizier_cluster DROP COLUMN project_name;
