
#include "app/logger.h"

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"
#include "quill/sinks/JsonSink.h"

#include <charconv>
#include <cstdint>
#include <string_view>

#include <iostream>

bool GetStrEnv(const char* name, char* buffer, const size_t buffer_size) {
  const char* value = std::getenv(name);
  if (!value) {
    return false;
  }

  const size_t len = std::strlen(value);

  if (len >= buffer_size) {
    return false;
  }

  std::memcpy(buffer, value, len+1);
  return true;
}

// Custom JSON sink that overrides the default field names to follow
// OpenTelemetry conventions and outputs epoch milliseconds for timestamp.
class CustomJsonFileSink : public quill::JsonFileSink {
public:
  using quill::JsonFileSink::JsonFileSink;

protected:
  void generate_json_message(
      quill::MacroMetadata const* log_metadata,
      uint64_t log_timestamp,
      std::string_view thread_id,
      std::string_view /* thread_name */,
      std::string const& /* process_id */,
      std::string_view logger_name,
      quill::LogLevel /* log_level */,
      std::string_view log_level_description,
      std::string_view /* log_level_short_code */,
      std::vector<std::pair<std::string, std::string>> const* named_args,
      std::string_view /* log_message */,
      std::string_view /* log_statement */,
      char const* message_format /* message_format */) override {
    _json_message.clear();

    uint64_t const timestamp_ms = log_timestamp / 1000000ULL;

    int line_number = 0;
    {
      std::string_view const line_sv{log_metadata->line()};
      std::from_chars(line_sv.data(), line_sv.data() + line_sv.size(), line_number);
    }

    uint64_t thread_id_value = 0;
    {
      std::from_chars(thread_id.data(), thread_id.data() + thread_id.size(), thread_id_value);
    }

    _json_message.append(fmtquill::format(
        R"({{"timestamp":{},"file_name":"{}","line":{},"thread_id":{},"logger":"{}","severity":"{}","message":"{}")",
        timestamp_ms,
        log_metadata->file_name(),
        line_number,
        thread_id_value,
        logger_name,
        log_level_description,
        message_format));

    if (named_args) {
      for (auto const& [key, value] : *named_args) {
        _json_message.append(fmtquill::format(R"(,"{}":"{}")", key, value));
      }
    }

    //_json_message.push_back('}');
  }
};

void QuillLogTest() {
  // Start the backend thread
  quill::BackendOptions backend_options;
  quill::Backend::start(backend_options);

  quill::FileSinkConfig file_cfg;
  file_cfg.set_open_mode('w');
  file_cfg.set_filename_append_option(quill::FilenameAppendOption::None);

  quill::ConsoleSinkConfig console_cfg;
  quill::ConsoleSinkConfig::Colours colours;
  colours.assign_colour_to_log_level(quill::LogLevel::Info, quill::ConsoleSinkConfig::Colours::blue);
  console_cfg.set_colours(colours);

  static constexpr size_t kBufferSize{256};
  char buffer[kBufferSize];

  if (!GetStrEnv("HOME", buffer, kBufferSize)) {
    return;
  }

  char path[kBufferSize];
  static constexpr auto kLogSuffix = "%s/tel/log/app.log";
  const int written = std::snprintf(path, kBufferSize, kLogSuffix, buffer);
  if (written < 0 || static_cast<size_t>(written) >= kBufferSize) {
    return;
  }

  const char* sink_path = path;

  // Create a json file for output
  auto json_sink = quill::Frontend::create_or_get_sink<CustomJsonFileSink>(
    sink_path, //TODO Change from using static path.
    file_cfg,
    quill::FileEventNotifier{});

  // It is also possible to create a logger than logs to both the json file and stdout
  // with the appropriate format
  auto json_sink_2 = quill::Frontend::get_sink(sink_path);
  auto console_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console_sink_id_1", console_cfg);

  // Define a custom format pattern for console logging, which includes named arguments in the output.
  // If you prefer to omit named arguments from the log messages, you can remove the "[%(named_args)]" part.
  quill::PatternFormatterOptions console_log_pattern{
    "[%(time)] [%(logger)] [%(thread_id)] [%(short_source_location)] [%(log_level)] %(message)",
    "%H:%M:%S.%Qus",
    quill::Timezone::LocalTime
};

  // Create a logger named "hybrid_logger" that writes to both a JSON sink and a console sink.
  // Note: The JSON sink uses its own internal format, so the custom format defined here
  // will only apply to the console output (via console_sink).
  quill::Logger* hybrid_logger = quill::Frontend::create_or_get_logger(
    "hybrid_logger", {std::move(json_sink_2), std::move(console_sink)}, console_log_pattern);

  LOG_INFO(hybrid_logger, "Set up path {path}", path);

  for (int i = 2; i < 4; ++i){
    LOG_INFO(hybrid_logger, "{method} to {endpoint} took {elapsed} ms", "POST", "http://", 10 * i);
  }

  LOG_INFO(hybrid_logger, "Operation {name} completed with code {code}, Data synced successfully", "Update", 123);
  LOG_DEBUG(hybrid_logger, "This is debug", "");
  LOG_INFO(hybrid_logger, "This is info", "");
  LOG_WARNING(hybrid_logger, "This is warning", "");
  LOG_ERROR(hybrid_logger, "This is error", "");

}
