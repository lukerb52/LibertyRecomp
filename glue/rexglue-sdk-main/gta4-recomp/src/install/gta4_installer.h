#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "gta4_source_inspector.h"

namespace gta4::install {

enum class Episode {
  kTlad,
  kTbogt,
};

struct DlcSelection {
  Episode episode;
  std::filesystem::path source;
};

struct Selection {
  std::filesystem::path game_source;
  std::filesystem::path update_source;
  std::vector<DlcSelection> dlc_sources;
};

struct Progress {
  std::atomic<uint64_t> copied_bytes{0};
  std::atomic<uint64_t> total_bytes{0};
  std::atomic<bool> cancel_requested{false};
  // Called synchronously by Install: may block for UI cancellation confirmation.
  // The caller must not mutate this callback while installation is running.
  std::function<bool()> should_continue;

  bool CheckContinue() {
    if (cancel_requested.load(std::memory_order_relaxed)) {
      return false;
    }
    if (should_continue && !should_continue()) {
      cancel_requested = true;
    }
    return !cancel_requested.load(std::memory_order_relaxed);
  }
};

struct Result {
  bool success = false;
  std::string error;
};

// Validate the pinned retail base and v8 delta without copying any files.
Result ValidateSelection(const Selection& selection);
bool IsInstallReady(const std::filesystem::path& game_root, std::string* reason = nullptr);
bool IsEpisodeReady(const std::filesystem::path& marketplace_root, Episode episode);
Result VerifyInstall(const std::filesystem::path& game_root,
                     const std::filesystem::path& marketplace_root);
Result Install(const Selection& selection, const std::filesystem::path& install_root,
               Progress& progress);

const char* EpisodeDirectory(Episode episode);

}  // namespace gta4::install
