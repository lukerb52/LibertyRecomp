#include <os/logger.h>
#include <rex/logging.h>
#include <rex/diagnostics/policy.h>

#include <spdlog/sinks/ansicolor_sink.h>

void os::logger::PlatformInitSinks()
{
    if (!rex::diagnostics::IsEnabled(
            rex::diagnostics::Category::kLogging)) return;
    auto sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
    rex::AddSink(sink);
}

void os::logger::Init()
{
    if (!rex::diagnostics::IsEnabled(
            rex::diagnostics::Category::kLogging)) return;
    rex::LogConfig config;
    config.log_to_console = false;  // PlatformInitSinks owns the console sink.
    config.default_level = spdlog::level::trace;
    config.flush_level = spdlog::level::trace;
    rex::InitLogging(config);
    PlatformInitSinks();
}

void os::logger::Shutdown()
{
    PlatformShutdownSinks();
}

void os::logger::PlatformShutdownSinks()
{
    // spdlog handles sink cleanup on shutdown; nothing to do here.
}
