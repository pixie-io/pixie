#!/usr/bin/env bash

set -ex

# Create an admin user (remove all args after app if you want to manually set)
fabmanager create-admin  \
    --username admin \
    --password admin \
    --app superset \
    --firstname user \
    --email admin@fab.org \
    --lastname Admin

# Initialize the database
superset db upgrade

# Load some data to play with
superset load_examples

# Create default roles and permissions
superset init

# Need to run `npm run build` when enter contains for first time
cd superset/assets && npm run build && cd ../../

# Start superset worker for SQL Lab
superset worker &

# Start the dev web server
flask run -p 8080 --with-threads --reload --debugger --host=0.0.0.0
