ALTER TABLE data_retention_plugin_releases ALTER COLUMN allow_insecure_tls SET DEFAULT false;
ALTER TABLE org_data_retention_plugins ALTER COLUMN insecure_tls SET DEFAULT false;
