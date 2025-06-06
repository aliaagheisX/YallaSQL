#pragma once

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/FileSink.h"
#include <string>
#include <utility>

namespace YallaSQL
{
    inline quill::Logger* getLogger(const std::string& logPath) {
        quill::Backend::start();

        auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(
            "./logs/",
            [] {
                quill::FileSinkConfig cfg;
                cfg.set_open_mode('w');
                cfg.set_filename_append_option(quill::FilenameAppendOption::StartDateTime);
                return cfg;
            }(),
            quill::FileEventNotifier{}
        );

        auto* logger = quill::Frontend::create_or_get_logger("root", std::move(file_sink));
        // Change the LogLevel to print everything
        logger->set_log_level(quill::LogLevel::TraceL3);
        return logger;
    }
    // quill::Logger* yalla_logger = getLogger("");

}
