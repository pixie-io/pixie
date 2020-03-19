DROP TABLE sites;

CREATE TABLE projects (
  -- org_id is the owner of this project.
  org_id UUID,
  -- project_name is the name of the project.
  project_name varchar(50),
  UNIQUE (org_id, project_name)
);
