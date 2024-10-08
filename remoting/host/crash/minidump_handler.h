// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CRASH_MINIDUMP_HANDLER_H_
#define REMOTING_HOST_CRASH_MINIDUMP_HANDLER_H_

#include "base/task/single_thread_task_runner.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/host/crash/crash_directory_watcher.h"
#include "remoting/host/crash/crash_file_uploader.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"

namespace remoting {

// A helper class which consolidates the functionality needed to watch for and
// upload minidumps generated by Breakpad.
class MinidumpHandler {
 public:
  MinidumpHandler();
  MinidumpHandler(const MinidumpHandler&) = delete;
  MinidumpHandler& operator=(const MinidumpHandler&) = delete;
  ~MinidumpHandler();

 private:
  // Use the same task_runner for URL load operations to prevent the underlying
  // Mojo objects from binding to different sequences and causing problems
  // during destruction.
  network::TransitionalURLLoaderFactoryOwner url_loader_factory_owner_{
      base::MakeRefCounted<URLRequestContextGetter>(
          base::SingleThreadTaskRunner::GetCurrentDefault())};
  CrashFileUploader crash_file_uploader_{
      url_loader_factory_owner_.GetURLLoaderFactory(),
      base::SingleThreadTaskRunner::GetCurrentDefault()};
  CrashDirectoryWatcher crash_directory_watcher_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CRASH_MINIDUMP_HANDLER_H_
