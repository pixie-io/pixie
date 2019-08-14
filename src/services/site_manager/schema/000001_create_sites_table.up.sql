CREATE TABLE sites (
  -- org_id is the owner of this site.
  org_id UUID,
  -- domain_name is the unique part of the domain name.
  domain_name varchar(50) UNIQUE
);
