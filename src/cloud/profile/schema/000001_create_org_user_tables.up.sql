CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

CREATE TABLE orgs (
  id UUID UNIQUE DEFAULT uuid_generate_v4(),
  org_name varchar(50) UNIQUE,
  domain_name varchar(50) UNIQUE,

  PRIMARY KEY(id)
);

CREATE TABLE users (
  id UUID UNIQUE DEFAULT uuid_generate_v4(),
  org_id UUID,
  username varchar(50),
  first_name varchar(50),
  last_name varchar(50),
  email varchar(100),

  PRIMARY KEY(id),
  FOREIGN KEY (org_id) REFERENCES orgs(id)
);
