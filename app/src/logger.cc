
#include "app/logger.h"

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"
#include "quill/sinks/JsonSink.h"

void QuillLogTest() {
    // Start the backend thread
    quill::BackendOptions backend_options;
    quill::Backend::start(backend_options);

  // Create a json file for output
  auto json_sink = quill::Frontend::create_or_get_sink<quill::JsonFileSink>(
    "example_json.log",
    []()
    {
      quill::FileSinkConfig cfg;
      cfg.set_open_mode('w');
      cfg.set_filename_append_option(quill::FilenameAppendOption::None);
      return cfg;
    }(),
    quill::FileEventNotifier{});

  // It is also possible to create a logger than logs to both the json file and stdout
  // with the appropriate format
  auto json_sink_2 = quill::Frontend::get_sink("example_json.log");
  auto console_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console_sink_id_1");

  // Define a custom format pattern for console logging, which includes named arguments in the output.
  // If you prefer to omit named arguments from the log messages, you can remove the "[%(named_args)]" part.
  quill::PatternFormatterOptions console_log_pattern = quill::PatternFormatterOptions{
    "[%(time)] [%(thread_id)] [%(short_source_location:<1)] [LOG_%(log_level)] [%(logger)] "
    "%(message)"};

  // Create a logger named "hybrid_logger" that writes to both a JSON sink and a console sink.
  // Note: The JSON sink uses its own internal format, so the custom format defined here
  // will only apply to the console output (via console_sink).
  quill::Logger* hybrid_logger = quill::Frontend::create_or_get_logger(
    "hybrid_logger", {std::move(json_sink_2), std::move(console_sink)}, console_log_pattern);

  for (int i = 2; i < 4; ++i)
  {
    LOG_INFO(hybrid_logger, "{method} to {endpoint} took {elapsed} ms", "POST", "http://", 10 * i);
  }

  LOG_INFO(hybrid_logger, "Operation {name} completed with code {code}", "Update", 123,
           "Data synced successfully");

}
