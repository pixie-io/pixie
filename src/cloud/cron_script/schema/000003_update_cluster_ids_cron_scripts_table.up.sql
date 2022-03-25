ALTER TABLE cron_scripts DROP COLUMN cluster_ids;
ALTER TABLE cron_scripts ADD COLUMN cluster_ids bytea;
