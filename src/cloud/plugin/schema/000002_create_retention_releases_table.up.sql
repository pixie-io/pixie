CREATE TABLE data_retention_plugin_releases (
  -- The unique identifier for the plugin.
  plugin_id varchar(1024) NOT NULL,
  -- The semVer version of the plugin release that these configurations belong to.
  version varchar(1024) NOT NULL,
  -- The set of configurations that the user needs to specify in order to configure the data retention plugin.
  configurations json NOT NULL DEFAULT '{}',
  -- A list of preset scripts, prewritten by the provider which the user can enable. Each JSON is expected to contain name, script, and default frequency.
  preset_scripts json[],
  -- A URL which points to a page providing documentation about the plugin provider's data retention plugin.
  documentation_url varchar(65536),
  -- The default export endpoint which data should be sent to.
  default_export_url varchar(65536),
  -- Whether users can specify a custom URL to which to send their scripts.
  allow_custom_export_url boolean,

  PRIMARY KEY (plugin_id, version),
  UNIQUE (plugin_id, version),
  FOREIGN KEY (plugin_id, version) REFERENCES plugin_releases(id, version)
);
