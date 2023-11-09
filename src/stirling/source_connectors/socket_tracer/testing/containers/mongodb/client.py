import pymongo

print("Starting MongoDB client")

client = pymongo.MongoClient("mongodb://localhost:27017/?timeoutMS=2000")

db = client["cartDB"]
collection = db["cart"]

# Insert document
doc = { "dessert": "cake" }
collection.insert_one(doc)

# Find document
collection.find_one()

# Update document
new_doc = { "$set": {"dessert": "ice cream"} }
resp = collection.update_one(doc, new_doc)

# Find updated document
resp = collection.find_one()

# Delete document
resp = collection.delete_one({"dessert": "ice cream"})