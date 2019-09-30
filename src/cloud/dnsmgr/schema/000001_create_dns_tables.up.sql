CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- This table contains information about our SSL certs and which clusters are using them.
CREATE TABLE ssl_certs (
  -- The cname for the cert.
  cname varchar(100),
  -- cluster_id is the id of the cluster using the cert.
  cluster_id UUID,
  -- cert is the cert for the SSL cert.
  cert varchar(8192),
  -- key is the key for the SSL cert.
  key varchar(8192)
);


CREATE TABLE dns_addresses (
  -- cluster_id is the id of the cluster.
  cluster_id UUID NOT NULL,
  -- time_created is the sequence number to use for the dns address.
  time_created TIMESTAMP,
  -- ip_address is the current IP address of the cluster.
  address varchar(1000),

  PRIMARY KEY(cluster_id, time_created)
);
