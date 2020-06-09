UPDATE vizier_cluster
SET cluster_name = regexp_replace(cluster_name, E'[\\n\\r]+', ' ', 'g' );
