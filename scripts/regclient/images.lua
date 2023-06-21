local images = {}

local defaultSourceRegistry = "gcr.io/pixie-oss/pixie-prod"
local defaultDestinationRegistries = {
  "docker.io/pxio",
  "ghcr.io/pixie-io",
  "quay.io/pixie",
}

local function imgCombine(registry, image)
  if string.find(registry, "gcr.io", 1, true) then
    return registry .. "/" .. image
  end
  return registry .. "/" .. string.gsub(image,"/","-")
end

local function isNonRCSemver(tag)
  -- RC, skip
  if string.find(tag, "-") then
    return false
  end
  -- not semver, skip
  if not string.find(tag, ".", 1, true) then
    return false
  end
  return true
end

function images.mirror(path, source, destinations)
  source = source or defaultSourceRegistry
  destinations = destinations or defaultDestinationRegistries

  local srcRef = reference.new(imgCombine(source, path))
  local tags = tag.ls(srcRef)
  -- loop through tags on each image
  for _, t in ipairs(tags) do
    if isNonRCSemver(t) then
      srcRef:tag(t)
      -- loop through destinations
      for _, destination in ipairs(destinations) do
        local destRef = reference.new(imgCombine(destination, path))
        destRef:tag(t)
        if not image.ratelimitWait(srcRef, 100) then
          error "Timed out waiting on rate limit"
        end
        image.copy(srcRef, destRef)
      end
    end
  end
end

return images
