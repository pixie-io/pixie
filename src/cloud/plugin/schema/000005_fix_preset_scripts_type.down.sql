ALTER TABLE data_retention_plugin_releases DROP COLUMN preset_scripts;
ALTER TABLE data_retention_plugin_releases ADD preset_scripts json[];
