CREATE TABLE plugin_releases (
  -- Human-readable name for the plugin.
  name varchar(1024) NOT NULL,
  -- A unique identifier for the plugin, specified by the plugin writer.
  id varchar(1024) NOT NULL,
  -- A description about the plugin.
  description varchar(1024),
  -- SVG for the logo to use for the plugin.
  logo text,
  -- The semVer version of the plugin release.
  version varchar(1024) NOT NULL,
  -- The timestamp at which the plugin was updated.
  updated_at TIMESTAMP,
  -- Whether data retention is enabled for the plugin or not.
  data_retention_enabled boolean,

  PRIMARY KEY (id, version),
  UNIQUE (id, version)
);
