#pragma once

#ifndef __linux__

#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace fs_watcher {

class FSWatcher {
 public:
  struct FSEvent {};
  static std::unique_ptr<FSWatcher> Create() { return std::unique_ptr<FSWatcher>(nullptr); }
  static bool SupportsInotify() { return false; }
  bool HasEvents() { return false; }
  bool HasOverflow() { return false; }
  bool NotInitialized() { return true; }
  StatusOr<FSEvent> GetNextEvent() { return error::NotImplemented("Inotify not supported"); }
  Status AddWatch(const fs::path &file_or_dir, uint32_t flags) {
    return error::NotImplemented("Inotify not supported");
  }
  Status RemoveWatch(const fs::path &file_or_dir) {
    return error::NotImplemented("Inotify not supported");
  }
  Status ReadInotifyUpdates() { return error::NotImplemented("Inotify not supported."); }
};
}  // namespace fs_watcher
}  // namespace stirling
}  // namespace pl

#else

#include <errno.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <unistd.h>
#include <experimental/filesystem>

#include <istream>
#include <iterator>
#include <memory>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace fs_watcher {

namespace fs = std::experimental::filesystem;

class FSWatcher {
 public:
  FSWatcher() = default;
  ~FSWatcher() { close(inotify_fd_); }

  static bool SupportsInotify() { return true; }

  static std::unique_ptr<FSWatcher> Create() {
    std::unique_ptr<FSWatcher> retval(new FSWatcher());
    return retval;
  }

  /**
   * @brief Enum class to report inotify event types. Currently we only support
   * overflow, creation/deletion of directories, and modification of files.
   * This can be extended if additional events need to be supported.
   */
  enum class FSEventType { kUnknown = 0, kOverFlow, kCreateDir, kDeleteDir, kModifyFile };

  /**
   * @brief Struct to define an FSNode in a FSNode tree. Each file or dir is
   * represented as an FSNode.
   *
   */
  struct FSNode {
    FSNode *parent;  // Raw pointer to parent. Does not imply ownership.
    std::vector<std::unique_ptr<FSNode> > children;
    int wd;            // Watch descriptor.
    std::string name;  // Name of the file or directory.
    FSNode(FSNode *node, int wd, std::string_view name) : parent(node), wd(wd), name(name) {}
  };

  /**
   * @brief Struct that is returned to a caller based on an Inotify event.
   * fs_node, wd, event_name help reconstruct the path or identify the
   * file responsible for an event in a caller, if necessary.
   *
   */
  struct FSEvent {
    FSEventType type;
    // Name of the event. For example, name of directory
    // that was created.
    std::string name;
    FSEvent(FSEventType type, std::string_view name, FSNode *node)
        : type(type), name(name), fs_node_(node) {}
    FSEvent() {}
    fs::path GetPath();

   private:
    FSNode *fs_node_;
  };

  /**
   * @brief Are there any pending inotify events that need to be processed.
   *
   * @return true Inotify events in queue.
   * @return false No Inotify events.
   */
  bool HasEvents();

  bool HasOverflow();

  void ResetOverflow();

  /**
   * @brief Get the Next Event object from the inptify queue if there are events
   * available. Caller is responsible to check HasEvents() before calling
   * this function.
   *
   * @return StatusOr<FSEvent> Return FSEvent if available. Return error
   * otherwise.
   */
  StatusOr<FSEvent> GetNextEvent();

  /**
   * @brief Add an inotify watch for a given path and flags.
   *
   * @param file_or_dir full path to be monitored.
   * @return Status
   */
  Status AddWatch(const fs::path &file_or_dir);

  /**
   * @brief Remove an existing watch for a path.
   *
   * @param file_or_dir full path
   * @return Status
   */
  Status RemoveWatch(const fs::path &file_or_dir);

  /**
   * @brief Read the inotify event queue and generate events for watchers that
   * have been added. Any new events will be available in the event_queue.
   *
   * @return Status
   */
  Status ReadInotifyUpdates();

  size_t NumEvents() const { return event_queue_.size(); }

  size_t NumWatchers() const { return inotify_watchers_.size(); }

  bool NotInitialized() { return inotify_fd_ == -1 || inotify_watchers_.empty(); }

 private:
  /**
   * @brief Initialize inotify and set up the FSNode tree to monitor a
   * filesystem. Called the first time AddWatch or ReadInotiyUpdates is called.
   *
   * @return Status
   */
  Status Init();

  /**
   * @brief Helper function to handle an inotify event.
   *
   * @param event
   * @return Status
   */
  Status HandleInotifyEvent(inotify_event *event);

  /**
   * @brief A struct to define the location of an FSNode in the FSNode tree.
   *
   */
  struct FSNodeLocation {
    FSNode *node;  // Does not imply ownership.
    std::vector<std::string_view>::iterator sv_it;
    FSNodeLocation(FSNode *node, std::vector<std::string_view>::iterator sv_it)
        : node(node), sv_it(sv_it) {}
  };

  /**
   * @brief Given a path, find the deepest FSNode that exists in the FSNode tree
   * for that path.
   * @param parent_node Pointer to a parent FSNode.
   * @param path_begin  String view vector iterator indicating beginning of
   * path.
   * @param path_end    String view vector iterator indicating end of path.
   * @return FSNodeLocation
   */
  FSNodeLocation LastFSNodeInPath(FSNode *parent_node,
                                  std::vector<std::string_view>::iterator path_begin,
                                  std::vector<std::string_view>::iterator path_end);

  /**
   * @brief Given a parent FSNode*, find a child node in its immediate children
   * with a specific name.
   * @param parent_node
   * @param child_name
   * @return std::vector<std::unique_ptr<FSNode>>::iterator
   */
  std::vector<std::unique_ptr<FSNode> >::iterator FindChildNode(FSNode *parent_node,
                                                                std::string_view child_name);

  /**
   * @brief Helper function to create new FS nodes from a partial path.
   *
   * @param node FSNode* to create new FSNodes from.
   * @param begin iterator to beginning of the partial path
   * @param end iterator to end of a path.
   * @return FSNode* For the last level FSNode in the path.
   */
  FSNode *CreateFSNodesFromPartialPath(FSNode *node, std::vector<std::string_view>::iterator begin,
                                       std::vector<std::string_view>::iterator end);

  /**
   * @brief Helper function to remove an existing FS node. Find the node to be
   * removed. Remove all the watchers for that node's children recursively and
   * then remove the node.
   *
   * @param node
   * @param path_begin
   * @param path_end
   * @return Status
   */
  Status RemoveFSNode(FSNode *node, std::vector<std::string_view>::iterator path_begin,
                      std::vector<std::string_view>::iterator path_end);

  /**
   * @brief Helper function to remove watchers for children of deleted nodes.
   *
   * @param node
   * @return Status
   */
  Status DeleteChildWatchers(FSNode *node);

  // Raw pointer to FSNode. Does not imply ownership. The node is owned by its
  // parent which holds a unique pointer to it.
  std::unordered_map<int, FSNode *> inotify_watchers_;
  std::queue<FSEvent> event_queue_;
  int inotify_fd_ = -1;
  std::unique_ptr<FSNode> root_fs_node_ = nullptr;
  bool overflow_ = false;
};

}  // namespace fs_watcher
}  // namespace stirling
}  // namespace pl

#endif
