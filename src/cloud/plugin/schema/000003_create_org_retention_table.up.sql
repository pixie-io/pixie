CREATE TABLE org_data_retention_plugins (
  -- org_id is the org who has this plugin enabled/disabled.
  org_id UUID NOT NULL,
  -- plugin_id is the ID of the plugin which the org has enabled/disabled.
  plugin_id varchar(1024) NOT NULL,
  -- version specifies which plugin release the org has enabled.
  version varchar(1024) NOT NULL,
  -- Configurations contains the user-configured values for the plugin. This should follow the configurations specified in data_retention_plugin_releases.
  -- The value is an encrypted JSON.
  configurations bytea,

  PRIMARY KEY (org_id, plugin_id),
  UNIQUE (org_id, plugin_id),
  FOREIGN KEY (plugin_id, version) REFERENCES plugin_releases(id, version)
);
