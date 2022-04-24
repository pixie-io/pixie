DROP TABLE IF EXISTS plugin_retention_scripts;

-- Revert back to previous table schema.
CREATE TABLE plugin_retention_scripts (
  -- org_id is the org who has this script enabled.
  org_id UUID NOT NULL,
  -- plugin_id is the ID of the plugin that the script is enabled for.
  plugin_id varchar(1024) NOT NULL,
  -- version is the version of the plugin that the script is enabled for.
  plugin_version varchar(1024) NOT NULL,
  -- script_id is the ID of the script object.
  script_id UUID NOT NULL,
  -- script_name is the name of the script.
  script_name varchar(1024) NOT NULL,
  -- description is a description of the script.
  description varchar(65536),
  -- contents contains the actual PxL script.
  contents varchar,
  -- frequency_s is how often the script should run, in seconds.
  frequency_s int,
  -- export_url is the URL which the data will be sent to.
  export_url varchar(65536),
  -- cluster_ids is the list of clusters which this script should run on. If empty, assumes it runs on all clusters in the org.
  cluster_ids UUID[],
  -- enabled is where the script is enabled for retention.
  enabled boolean,
  -- is_preset is whether or not the script is a preset script provided by the plugin provider.
  is_preset boolean,

  PRIMARY KEY (script_id),
  UNIQUE (org_id, script_name),
  FOREIGN KEY (plugin_id, plugin_version) REFERENCES plugin_releases(id, version)
);
