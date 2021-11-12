UPDATE orgs SET domain_name = org_name;

DROP INDEX idx_orgs_domain_name;

ALTER TABLE orgs ADD CONSTRAINT orgs_domain_name_key UNIQUE (domain_name);
