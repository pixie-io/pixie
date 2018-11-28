case node['platform']
when 'mac_os_x'
  include_recipe 'pixielabs::mac_os_x_desktop'
else
  include_recipe 'pixielabs::linux_desktop'
end
