PROD_APPS_PREFIX := gs://pixie-prod-artifacts/prod-demo-apps
STAGING_APPS_PREFIX := gs://pixie-prod-artifacts/staging-demo-apps
ARTIFACTS_DIR := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))/artifacts

# Rename namespace sock-shop to be px-sock-shop.
.PHONY: package_sock_shop
package_sock_shop:
	cp -r applications/sockshop/kubernetes_manifests $(ARTIFACTS_DIR)/tmp/sock_shop_manifests
	# replace namespace sock-shop with namespace px-sock-shop
	find $(ARTIFACTS_DIR)/tmp/sock_shop_manifests -type f -print0 | xargs -0 sed -i 's/sock-shop/px-sock-shop/g'

	# tar up the edited files
	tar -czf $(ARTIFACTS_DIR)/px-sock-shop.tar.gz -C $(ARTIFACTS_DIR)/tmp/sock_shop_manifests .

.PHONY: package_demo_apps
package_demo_apps: 
	rm -rf $(ARTIFACTS_DIR)
	mkdir -p $(ARTIFACTS_DIR)
	mkdir -p $(ARTIFACTS_DIR)/tmp
# TODO(nserrino): Add in other demo applications once they have working load testers.
	make package_sock_shop

update_bundle: package_demo_apps
# Requires prod access to update.
	gsutil -h "Cache-Control:no-cachle,max-age=0" \
               -h "Content-Type:application/json" \
               cp $(PWD)/manifest.json $(PREFIX)/manifest.json

	gsutil -h "Cache-Control:no-cachle,max-age=0" \
               -h "Content-Type:application/gzip" \
               cp $(ARTIFACTS_DIR)/px-sock-shop.tar.gz $(PREFIX)/px-sock-shop.tar.gz

# Readable by everyone
	gsutil acl ch -u allUsers:READER $(PREFIX)/manifest.json $(PREFIX)/px-sock-shop.tar.gz

.PHONY: update_prod_bundle
update_prod_bundle:
	PREFIX=$(PROD_APPS_PREFIX) make update_bundle

.PHONY: update_staging_bundle
update_staging_bundle:
	PREFIX=$(STAGING_APPS_PREFIX) make update_bundle
