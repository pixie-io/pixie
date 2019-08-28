CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- This table contains cluster registration information, access control, etc.
CREATE TABLE vizier_cluster (
  -- The ID to use for this cluster.
  id UUID UNIQUE DEFAULT uuid_generate_v4(),
  -- org_id is the owner of this cluster.
  org_id UUID NOT NULL,
  -- Timestamp when this cluster was created.
  created_at TIMESTAMP DEFAULT NOW(),
  -- Timestamp we have first seen this cluster.
  first_seen_at TIMESTAMP,

  PRIMARY KEY(id)
);


CREATE TYPE vizier_status AS ENUM ('UNKNOWN', 'HEALTHY', 'UNHEALTHY', 'DISCONNECTED');

CREATE TABLE vizier_cluster_info (
  -- The ID to use for this cluster.
  vizier_cluster_id UUID NOT NULL,
  -- The time of the last heartbeat.
  last_heartbeat TIMESTAMP,
  -- The IP address of the cluster.
  address varchar(1000),
  -- The signing key of the cluster.
  jwt_signing_key varchar(1000),

  status vizier_status DEFAULT 'DISCONNECTED',

  PRIMARY KEY(vizier_cluster_id),
  FOREIGN KEY(vizier_cluster_id) REFERENCES vizier_cluster(id)
);
