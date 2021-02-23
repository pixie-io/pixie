PROD_APPS_PREFIX := gs://pixie-prod-artifacts/prod-demo-apps
DEV_APPS_PREFIX := gs://pl-infra-dev-artifacts/dev-demo-apps
ARTIFACTS_DIR := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))/artifacts

# Rename namespace sock-shop to be px-sock-shop.
.PHONY: package_sock_shop
package_sock_shop:
	cp -r applications/sockshop/kubernetes_manifests $(ARTIFACTS_DIR)/tmp/sock_shop_manifests
	# replace namespace sock-shop with namespace px-sock-shop
	# When the namespace is missing, the demo app code will automatically add the correct
	# namespace. TODO(nserrino): We should consider handling this part in the demo app code
	# if we ever want to support configurable namespaces for demo apps.
	find $(ARTIFACTS_DIR)/tmp/sock_shop_manifests -type f -print0 | xargs -0 perl -pi -e s/sock-shop/px-sock-shop/g

	# tar up the edited files
	tar -czf $(ARTIFACTS_DIR)/px-sock-shop.tar.gz -C $(ARTIFACTS_DIR)/tmp/sock_shop_manifests .

.PHONY: package_online_boutique
package_online_boutique:
	cp -r applications/online-boutique $(ARTIFACTS_DIR)/tmp/online_boutique_manifests

	# tar up the edited files
	tar -czf $(ARTIFACTS_DIR)/px-online-boutique.tar.gz -C $(ARTIFACTS_DIR)/tmp/online_boutique_manifests .

.PHONY: package_demo_apps
package_demo_apps: 
	rm -rf $(ARTIFACTS_DIR)
	mkdir -p $(ARTIFACTS_DIR)
	mkdir -p $(ARTIFACTS_DIR)/tmp
# TODO(nserrino): Add in other demo applications once they have working load testers.
	make package_sock_shop
	make package_online_boutique

update_bundle: package_demo_apps
# Requires prod access to update.
	gsutil -h "Cache-Control:no-cachle,max-age=0" \
               -h "Content-Type:application/json" \
               cp $(PWD)/manifest.json $(PREFIX)/manifest.json

	gsutil -h "Cache-Control:no-cachle,max-age=0" \
               -h "Content-Type:application/gzip" \
               cp $(ARTIFACTS_DIR)/px-sock-shop.tar.gz $(PREFIX)/px-sock-shop.tar.gz

	gsutil -h "Cache-Control:no-cachle,max-age=0" \
               -h "Content-Type:application/gzip" \
               cp $(ARTIFACTS_DIR)/px-online-boutique.tar.gz $(PREFIX)/px-online-boutique.tar.gz

# Readable by everyone
	gsutil acl ch -u allUsers:READER $(PREFIX)/manifest.json $(PREFIX)/px-sock-shop.tar.gz
	gsutil acl ch -u allUsers:READER $(PREFIX)/manifest.json $(PREFIX)/px-online-boutique.tar.gz

.PHONY: update_prod_bundle
update_prod_bundle:
	PREFIX=$(PROD_APPS_PREFIX) make update_bundle

.PHONY: update_dev_bundle
update_dev_bundle:
	PREFIX=$(DEV_APPS_PREFIX) make update_bundle
