# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

import pymongo

print("Starting MongoDB client")

client = pymongo.MongoClient("mongodb://localhost:27017/?timeoutMS=2000")

db = client["db"]
collection = db["name"]

# Insert document
doc = {"name": "foo"}
collection.insert_one(doc)

# Find document
collection.find_one()

# Update document
new_doc = {"$set": {"name": "bar"}}
resp = collection.update_one(doc, new_doc)

# Find updated document
resp = collection.find_one()

# Delete document
resp = collection.delete_one({"name": "bar"})
