require 'tmpdir'

current_dir       = File.expand_path(File.dirname(__FILE__))
file_cache_path   Dir.tmpdir()
node_path         Dir.tmpdir()
cookbook_path     [
  "#{current_dir}/site-cookbooks",
  "#{current_dir}/cookbooks",
]
role_path         "#{current_dir}/roles"
data_bag_path     "#{current_dir}/data_bags"
json_attribs      "#{current_dir}/node.json"
