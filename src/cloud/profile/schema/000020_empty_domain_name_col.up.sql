ALTER TABLE orgs DROP CONSTRAINT orgs_domain_name_key;

CREATE INDEX idx_orgs_domain_name ON orgs(domain_name);

UPDATE orgs SET domain_name = NULL;
