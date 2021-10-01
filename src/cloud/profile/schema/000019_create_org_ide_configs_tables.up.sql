CREATE TABLE org_ide_configs (
  org_id UUID,
  ide_name VARCHAR (1024),
  path TEXT,

  PRIMARY KEY(org_id, ide_name),
  UNIQUE (org_id, ide_name),
  FOREIGN KEY (org_id) REFERENCES orgs(id)
);
