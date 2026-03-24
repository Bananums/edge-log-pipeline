
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
      std::string_view log_message,
      std::string_view /* log_statement */,
      char const* message_format) override {
    _json_message.clear();

    uint64_t const timestamp_ms = log_timestamp / 1000000ULL;
    int const line_number = ParseIntOrDefault<int>(log_metadata->line(), 0);
    uint64_t const thread_id_value = ParseIntOrDefault<uint64_t>(thread_id, 0);

    _json_message.push_back('{');

    AppendJsonUIntField(_json_message, "timestamp", timestamp_ms);
    AppendJsonStringField(_json_message, "file_name", log_metadata->file_name());
    AppendJsonIntField(_json_message, "line", line_number);
    AppendJsonUIntField(_json_message, "thread_id", thread_id_value);
    AppendJsonStringField(_json_message, "logger", logger_name);
    AppendJsonStringField(_json_message, "severity", log_level_description);
    AppendJsonStringField(_json_message, "message", log_message);
    AppendJsonStringField( _json_message, "message_t",
      message_format ? std::string_view{message_format} : std::string_view{});

    if (named_args != nullptr) {
      for (auto const& [key, value] : *named_args) {
        AppendJsonStringField(_json_message, key, value);
      }
    }

    // Do not append '}' here.
    // Quill finalizes the JSON object after this method returns.
  }

 private:
  using Buffer = fmtquill::memory_buffer;

  template <typename IntType>
  static IntType ParseIntOrDefault(std::string_view sv, IntType default_value) {
    IntType value = default_value;
    auto const* begin = sv.data();
    auto const* end = sv.data() + sv.size();
    auto const result = std::from_chars(begin, end, value);

    if ((result.ec != std::errc{}) || (result.ptr != end)) {
      return default_value;
    }

    return value;
  }

  static void AppendRaw(Buffer& out, std::string_view value) {
    out.append(value.data(), value.data() + value.size());
  }

  static void AppendJsonEscaped(Buffer& out, std::string_view value) {
    for (char ch : value) {
      switch (ch) {
        case '"':
          AppendRaw(out, "\\\"");
          break;
        case '\\':
          AppendRaw(out, "\\\\");
          break;
        case '\b':
          AppendRaw(out, "\\b");
          break;
        case '\f':
          AppendRaw(out, "\\f");
          break;
        case '\n':
          AppendRaw(out, "\\n");
          break;
        case '\r':
          AppendRaw(out, "\\r");
          break;
        case '\t':
          AppendRaw(out, "\\t");
          break;
        default: {
          unsigned char const uch = static_cast<unsigned char>(ch);
          if (uch < 0x20U) {
            fmtquill::format_to(std::back_inserter(out), "\\u{:04X}", uch);
          } else {
            out.push_back(ch);
          }
          break;
        }
      }
    }
  }

  static void AppendFieldPrefix(Buffer& out, std::string_view key) {
    if (out.size() > 1) {
      out.push_back(',');
    }

    out.push_back('"');
    AppendJsonEscaped(out, key);
    AppendRaw(out, "\":");
  }

  static void AppendJsonStringField(
      Buffer& out,
      std::string_view key,
      std::string_view value) {
    AppendFieldPrefix(out, key);
    out.push_back('"');
    AppendJsonEscaped(out, value);
    out.push_back('"');
  }

  static void AppendJsonStringField(
      Buffer& out,
      std::string_view key,
      char const* value) {
    AppendJsonStringField(
        out,
        key,
        value ? std::string_view{value} : std::string_view{});
  }

  template <typename IntType>
  static void AppendJsonIntField(
      Buffer& out,
      std::string_view key,
      IntType value) {
    AppendFieldPrefix(out, key);
    fmtquill::format_to(std::back_inserter(out), "{}", value);
  }

  template <typename UIntType>
  static void AppendJsonUIntField(
      Buffer& out,
      std::string_view key,
      UIntType value) {
    AppendFieldPrefix(out, key);
    fmtquill::format_to(std::back_inserter(out), "{}", value);
  }
};

quill::Logger* GetLogger() {
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
    return nullptr;
  }

  char path[kBufferSize];
  static constexpr auto kLogSuffix = "%s/tel/log/app.log";
  const int written = std::snprintf(path, kBufferSize, kLogSuffix, buffer);
  if (written < 0 || static_cast<size_t>(written) >= kBufferSize) {
    return nullptr;
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

  return hybrid_logger;
}
