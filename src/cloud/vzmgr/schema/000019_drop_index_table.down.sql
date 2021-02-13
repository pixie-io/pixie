-- This table contains index state for each vizier.
CREATE TABLE vizier_index_state (
  -- The ID of the cluster.
  cluster_id UUID UNIQUE DEFAULT uuid_generate_v4(),
  resource_version varchar(50),

  PRIMARY KEY(cluster_id)
);

--- Create index state for existing Viziers.
INSERT INTO vizier_index_state(cluster_id, resource_version) SELECT id, '' FROM vizier_cluster;
