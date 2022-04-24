DROP TABLE IF EXISTS plugin_retention_scripts;

CREATE TABLE plugin_retention_scripts (
  -- org_id is the org who has this script enabled.
  org_id UUID NOT NULL,
  -- plugin_id is the ID of the plugin that the script is enabled for.
  plugin_id varchar(1024) NOT NULL,
  -- version is the version of the plugin that the script is enabled for.
  plugin_version varchar(1024) NOT NULL,
  -- script_id is the ID of the script object in the cron script service.
  script_id UUID NOT NULL,
  -- script_name is the name of the script.
  script_name varchar(1024) NOT NULL,
  -- description is a description of the script.
  description varchar(65536),
  -- export_url is the URL which the data will be sent to. Hashed using the database key for privacy.
  export_url bytea,
  -- is_preset is whether or not the script is a preset script provided by the plugin provider.
  is_preset boolean,

  PRIMARY KEY (script_id),
  UNIQUE (org_id, script_name),
  FOREIGN KEY (plugin_id, plugin_version) REFERENCES plugin_releases(id, version)
);
