PIXIE_CLI := px
# Update dir name here if you want to add a new directory.
dirs := px sotw
script_files := $(foreach dir,$(dirs),$(wildcard $(dir)/**/*))
BUNDLE := gs://pixie-prod-artifacts/script-bundles/bundle-oss.json
EXECUTABLES = $(PIXIE_CLI) gsutil
K := $(foreach exec,$(EXECUTABLES),\
        $(if $(shell which $(exec)),some string,$(error "No $(exec) in PATH")))

all: bundle-oss.json.gz

bundle-oss.json: $(script_files) $(PIXIE_CLI)
	@$(PIXIE_CLI) create-bundle --search_path $(PWD) $(foreach dir,$(dirs),--base $(dir)) -o $(PWD)/bundle-oss.json

bundle-oss.json.gz: bundle-oss.json
	gzip -c $< > $@

.PHONY: update_bundle
update_bundle: bundle-oss.json.gz
# Requires prod access to update, or needs to run in CI deploy.
	gsutil -h "Cache-Control:no-cachle,max-age=0" \
         -h "Content-Type:application/json" \
         -h "Content-Encoding: gzip" \
         cp $< $(BUNDLE)
# Readable by everyone
	gsutil acl ch -u allUsers:READER $(BUNDLE)

update_readme: 
	./update_readme.py

.PHONY: dev
dev:
	./watch.sh
