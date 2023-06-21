local utils = {}

local gcr = "gcr.io"

function utils.parseVersion(v)
  return string.match(v, "^(%d+)%.(%d+)%.(%d+)(.-)$")
end

function utils.combine(a, b)
  -- create a path that looks like a directory, this is supported by gcr
  local dirPath = a .. "/" .. b
  if string.sub(dirPath, 1, #gcr) == gcr then
    return dirPath
  end

  local i = string.find(dirPath, "/", 1, true)
  if i == nil then
    error "Image doesn't have any path delimiters"
  end

  local registry = string.sub(dirPath, 1, i-1)
  local rest = string.sub(dirPath, i+1)

  i = string.find(rest, "/", 1, true)
  if i == nil then
    -- this must be a library style repo with no namespace, e.g. docker.io/alpine
    return dirPath
  end

  local namespace = string.sub(rest, 1, i-1)
  local repo = string.sub(rest, i+1)

  return registry .. "/" .. namespace .. "/" .. string.gsub(repo, "/", "-")
end

return utils
