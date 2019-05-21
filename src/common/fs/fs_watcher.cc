#ifdef __linux__

#include <experimental/filesystem>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <memory>
#include <queue>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/str_split.h"
#include "src/common/base/error.h"
#include "src/common/fs/fs_watcher.h"

namespace pl {

namespace fs = std::experimental::filesystem;

const uint32_t kBufferSize = 4096;

fs::path FSWatcher::FSEvent::GetPath() {
  fs::path path = fs_node_->name;
  FSNode *node_ptr = fs_node_->parent;
  while (node_ptr) {
    path = node_ptr->name / path;
    node_ptr = node_ptr->parent;
  }
  return path;
}

bool FSWatcher::HasEvents() { return (!event_queue_.empty() && !overflow_); }

bool FSWatcher::HasOverflow() { return overflow_; }

void FSWatcher::ResetOverflow() { overflow_ = false; }

StatusOr<FSWatcher::FSEvent> FSWatcher::GetNextEvent() {
  if (event_queue_.size()) {
    FSEvent new_event = event_queue_.front();
    event_queue_.pop();
    return new_event;
  }
  return error::NotFound("Inotify event queue is empty. No event found.");
}

Status FSWatcher::Init() {
  inotify_fd_ = inotify_init1(IN_NONBLOCK);
  if (inotify_fd_ == -1) {
    return error::Unknown("Failed to initialize inotify");
  }
  // Setting the watch descriptor to -1,
  // which implies that it is invalid.
  root_fs_node_ = std::make_unique<FSNode>(nullptr, /* wd */ -1, "/");
  return Status::OK();
}

std::vector<std::unique_ptr<FSWatcher::FSNode>>::iterator FSWatcher::FindChildNode(
    FSNode *parent_node, std::string_view child_name) {
  auto it = std::find_if(
      parent_node->children.begin(), parent_node->children.end(),
      [&](std::unique_ptr<FSNode> &obj) { return obj->name.compare(child_name) == 0; });
  return it;
}

FSWatcher::FSNode *FSWatcher::CreateFSNodesFromPartialPath(
    FSNode *parent_node, std::vector<std::string_view>::iterator path_begin,
    std::vector<std::string_view>::iterator path_end) {
  auto current_dir_it = path_begin;
  FSNode *current_parent_node = parent_node;

  while (current_dir_it != path_end) {
    FSNode *next_parent_node = nullptr;
    // wd = -1 implies invalid watch descriptor.
    auto new_node = std::make_unique<FSNode>(current_parent_node, /* wd */ -1, *current_dir_it);
    next_parent_node = new_node.get();
    current_parent_node->children.emplace_back(std::move(new_node));

    current_parent_node = next_parent_node;
    ++current_dir_it;
  }

  return current_parent_node;
}

FSWatcher::FSNodeLocation FSWatcher::LastFSNodeInPath(
    FSNode *parent_node, std::vector<std::string_view>::iterator path_begin,
    std::vector<std::string_view>::iterator path_end) {
  auto current_dir_it = path_begin;
  while (current_dir_it != path_end) {
    auto child_it = FindChildNode(parent_node, *current_dir_it);
    if (child_it == parent_node->children.end()) {
      break;
    }
    parent_node = child_it->get();
    current_dir_it++;
  }

  return FSNodeLocation(parent_node, current_dir_it);
}

Status FSWatcher::AddWatch(const fs::path &file_or_dir) {
  if (!root_fs_node_) {
    PL_RETURN_IF_ERROR(Init());
  }
  std::string path_str = file_or_dir.string();
  std::vector<std::string_view> parsed_path =
      absl::StrSplit(path_str, fs::path::preferred_separator, absl::SkipWhitespace());

  // Find and/or construct required path in the FSNode tree.
  FSNode *node_ptr = nullptr;
  if (parsed_path.empty()) {
    node_ptr = root_fs_node_.get();
  } else {
    FSNodeLocation node_location =
        LastFSNodeInPath(root_fs_node_.get(), parsed_path.begin(), parsed_path.end());

    // Construct the required FSNodes for the given path from
    // node_location, if needed.
    node_ptr = node_location.node;
    if (node_location.sv_it != parsed_path.end()) {
      node_ptr =
          CreateFSNodesFromPartialPath(node_location.node, node_location.sv_it, parsed_path.end());
    }
  }

  // Add the watch and update related data structures.
  std::error_code ec;
  bool is_dir = fs::is_directory(file_or_dir, ec);
  if (ec) {
    return error::Unknown("Failed to determine if $0 is a dir: $1", path_str, ec.message());
  }
  // TODO(kgandhi): Make this configurable or define as constant in
  // header. For example, flags for directories, files, etc.
  uint32_t flags;
  flags = is_dir ? IN_CREATE | IN_DELETE | IN_Q_OVERFLOW : IN_ACCESS | IN_MODIFY | IN_Q_OVERFLOW;

  int wd = inotify_add_watch(inotify_fd_, path_str.c_str(), flags);
  if (wd == -1) {
    return error::Unknown("Failed to add $0 to the inotify watcher", path_str);
  }
  inotify_watchers_[wd] = node_ptr;
  node_ptr->wd = wd;
  return Status::OK();
}

Status FSWatcher::RemoveFSNode(FSNode *parent_node,
                               std::vector<std::string_view>::iterator path_begin,
                               std::vector<std::string_view>::iterator path_end) {
  auto current_dir_it = path_begin;

  FSNode *last_node = parent_node;
  std::vector<std::unique_ptr<FSNode>>::iterator child_node_it;

  // Find the FSNode that corresponds to the last dir or file in the given path.
  while (current_dir_it != path_end) {
    parent_node = last_node;
    child_node_it = FindChildNode(parent_node, *current_dir_it);
    if (child_node_it == parent_node->children.end()) {
      return error::NotFound("Could not find FS node to remove");
    }
    last_node = child_node_it->get();
    ++current_dir_it;
  }

  // Delete the last node in the path along with its children and watchers.
  PL_RETURN_IF_ERROR(DeleteChildWatchers(last_node));
  auto wd = last_node->wd;
  parent_node->children.erase(child_node_it);
  inotify_rm_watch(inotify_fd_, wd);
  inotify_watchers_.erase(wd);
  return Status::OK();
}

Status FSWatcher::DeleteChildWatchers(FSNode *node) {
  if (node->children.empty()) {
    return Status::OK();
  }
  for (const auto &it : node->children) {
    inotify_rm_watch(inotify_fd_, it->wd);
    inotify_watchers_.erase(it->wd);
    PL_RETURN_IF_ERROR(DeleteChildWatchers(it.get()));
  }
  return Status::OK();
}

Status FSWatcher::RemoveWatch(const fs::path &file_or_dir) {
  auto path_str = file_or_dir.string();
  std::vector<std::string_view> parsed_path =
      absl::StrSplit(path_str, fs::path::preferred_separator, absl::SkipWhitespace());
  PL_RETURN_IF_ERROR(RemoveFSNode(root_fs_node_.get(), parsed_path.begin(), parsed_path.end()));
  return Status::OK();
}

Status FSWatcher::HandleInotifyEvent(inotify_event *event) {
  FSEventType type = FSEventType::kUnknown;
  std::string_view event_name;

  if (event->len) {
    auto event_length = strnlen(event->name, event->len);
    DCHECK(event_length < event->len);
    event_name = std::string_view(event->name, event_length);
  }

  // Note that the events we are setting the type for are based on the
  // enum class defined in the header. This can be extended based on
  // other events that may be of interest. For now, we only report
  // directory creation or deletion and file modification.
  if (event->wd == -1) {
    type = FSEventType::kOverFlow;
    overflow_ = true;
  } else if (event->mask & IN_ISDIR) {
    if (event->mask & IN_CREATE) {
      type = FSEventType::kCreateDir;
    } else if (event->mask & IN_DELETE) {
      type = FSEventType::kDeleteDir;
    }
  } else if (event->mask & IN_MODIFY || event->mask & IN_ACCESS) {
    type = FSEventType::kModifyFile;
  }

  event_queue_.emplace(type, event_name, inotify_watchers_[event->wd]);
  return Status::OK();
}

Status FSWatcher::ReadInotifyUpdates() {
  if (!root_fs_node_) {
    PL_RETURN_IF_ERROR(Init());
  }

  char buffer[kBufferSize];
  int length = 0;
  size_t bytes_read = 0;
  size_t buffer_offset = 0;

  while ((length = read(inotify_fd_, buffer + bytes_read, sizeof(buffer) - bytes_read)) > 0) {
    bytes_read += length;
    while (!overflow_ && (buffer_offset < bytes_read)) {
      inotify_event *event = reinterpret_cast<inotify_event *>(&buffer[buffer_offset]);
      // Check if the read is partial.
      if (buffer_offset + sizeof(inotify_event) + event->len > bytes_read) {
        // memcpy partial event to the beginning of the buffer.
        int num_bytes_to_copy = bytes_read - buffer_offset;
        std::memcpy(buffer, event, num_bytes_to_copy);
        // Set bytes_read and bufer_offset due to memcpy to beginning of buffer.
        bytes_read = num_bytes_to_copy;
        buffer_offset = 0;
        break;
      }
      PL_RETURN_IF_ERROR(HandleInotifyEvent(event));
      buffer_offset += sizeof(inotify_event) + event->len;
    }
  }

  if (length == -1 && errno != EAGAIN) {
    return error::Unknown("Could not read from inotify file descriptor. Error code: $0", errno);
  }

  // length == 0 or length == -1 && errno == EAGAIN (no data to read)
  return Status::OK();
}

}  // namespace pl
#endif
