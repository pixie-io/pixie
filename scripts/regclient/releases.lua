local mirrors = require 'mirrors'
local utils = require 'utils'

local releases = {}

function releases.mirror(path)
  local srcRef = reference.new(utils.combine(mirrors.sourceRegistry, path))
  local tags = tag.ls(srcRef)
  -- loop through tags on each image
  for _, t in ipairs(tags) do
    local major, _, _, ext = utils.parseVersion(t)
    if major ~= nil and string.sub(ext, 1, 1) ~= "-" then
      srcRef:tag(t)
      -- loop through destinations
      for _, destination in ipairs(mirrors.destinationRegistries) do
        local destRef = reference.new(utils.combine(destination, path))
        destRef:tag(t)
        if not image.ratelimitWait(srcRef, 100) then
          error "Timed out waiting on rate limit"
        end
        image.copy(srcRef, destRef)
      end
    end
  end
end

return releases
