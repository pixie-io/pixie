PIXIE_CLI := px
# Update dir name here if you want to add a new directory.
dirs := px
script_files := $(foreach dir,$(dirs),$(wildcard $(dir)/**/*))

EXECUTABLES = $(PIXIE_CLI) gsutil
K := $(foreach exec,$(EXECUTABLES),\
        $(if $(shell which $(exec)),some string,$(error "No $(exec) in PATH")))

all: bundle-oss.json.gz

bundle-oss.json: $(script_files) $(PIXIE_CLI)
	@$(PIXIE_CLI) create-bundle --search_path $(PWD) $(foreach dir,$(dirs),--base $(dir)) -o $(PWD)/bundle-oss.json

bundle-oss.json.gz: bundle-oss.json
	gzip -c $< > $@

.PHONY: dev
dev:
	./watch.sh
