# Elasticsearch 7.6.0 to 8.x/9.x Upgrade Notes

## Important: Migration Path
You cannot upgrade directly from 7.6.0 to 8.x. Follow this path:
1. Upgrade from 7.6.0 to 7.17.x (latest patch)
2. Upgrade from 7.17.x to 8.x

## Changes Made to Manifests

### 1. elastic_cluster.yaml
- Removed custom image reference (ECK will use official Elastic images)
- Updated version from 7.6.0 to 8.15.0
- Changed node configuration from legacy format to new `node.roles` format:
  - Master nodes: `node.roles: ["master"]`
  - Data nodes: `node.roles: ["data", "ingest"]`
- Removed GCS plugin installation (now bundled with ES 8.x)
- Removed deprecated `-Dlog4j2.formatMsgNoLookups=True` JVM option

### 2. elastic_gcs_plugin.yaml
- This patch file is no longer needed as GCS plugin is bundled with ES 8.x
- File has been updated with a comment explaining this
- You should remove the patch reference from `k8s/cloud_deps/public/elastic/kustomization.yaml`

## Security Changes in ES 8.x
- Security is enabled by default
- TLS is required by default
- Authentication is enabled by default
- The `kibana` user is replaced by `kibana_system`

## Pre-Upgrade Checklist
- [ ] Ensure no indices from ES 6.x or earlier exist
- [ ] Update any `nGram`/`edgeNGram` tokenizers to `ngram`/`edge_ngram`
- [ ] Remove any custom `thread_pool.write.size` settings temporarily
- [ ] Backup your data before upgrading
- [ ] Test the upgrade in a non-production environment first

## Post-Upgrade Actions
1. Update client applications to handle security changes
2. Update Kibana configuration if using it
3. Remove the elastic_gcs_plugin.yaml patch from kustomization files
4. Monitor cluster health during rolling restart