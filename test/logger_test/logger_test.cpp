// NO_LINT_BEGIN
#define LOGGING_DEBUG_AND_ABOVE
#include "logger.h"  // Has to be included before "mock_wrapper.h"
// NO_LINT_END

#include <array>
#include <charconv>
#include <chrono>
#include <climits>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <gmock/gmock.h>
#include <iostream>
#include <optional>
#include <regex>

#if defined __linux__
#elif defined __APPLE__
# include <mach-o/dyld.h>
#elif defined _WIN32
#endif  // OS

#include "mdn/gtest_extension.hpp"
#include "mdn/mock_wrapper.hpp"
#include "mdn/standard_streams_redirection.h"

using namespace testing;
namespace fs = std::filesystem;

namespace {
static constexpr int YEAR_OFFSET  = 1900;
static constexpr int MONTH_OFFSET = 1;

// Thread-safe safeLocalTime helper: use localtime_r on POSIX, localtime_s on Windows.
struct tm safeLocalTime(std::time_t t) {
    struct tm result{};
#if defined(_WIN32)
    localtime_s(&result, &t);
#else
    localtime_r(&t, &result);
#endif
    return result;
}
}  // namespace

#undef MDN_LOGGER_FUNC_NAME
#define MDN_LOGGER_FUNC_NAME testFullName.c_str()

#define TEST_ANSI_ESC         "\033"
#define TEST_ANSI_PARAM_BEGIN "["
#define TEST_ANSI_PARAM_END   "m"
#define TEST_ANSI_COLOR(code) TEST_ANSI_ESC TEST_ANSI_PARAM_BEGIN code TEST_ANSI_PARAM_END

enum class OutputFiles {
    LOGGER_OUTPUT_1,
    LOGGER_OUTPUT_2,
    STDOUT_REDIRECTION,
    STDERR_REDIRECTION,
    COUNT,
};

struct FormatRegexIndices {
    std::optional<size_t> dateIndex;
    size_t                timeIndex = SIZE_MAX;
    std::optional<size_t> logLevelIndex;
    size_t                functionIndex = SIZE_MAX;
    size_t                messageIndex  = SIZE_MAX;
    std::optional<size_t> colorPrefixIndex;
    std::optional<size_t> colorSuffixIndex;
};

class BinaryFileReader {  // NOLINT(hicpp-special-member-functions)
public:
    explicit BinaryFileReader(const fs::path &filename) : filename_(filename) {
        file_.open(filename, std::ios::in | std::ios::binary);  // NOLINT(hicpp-signed-bitwise)
    }

    ~BinaryFileReader() noexcept {
        if (file_.is_open()) {
            closeStreamNoThrow(file_);
        }
    }

    BinaryFileReader(const BinaryFileReader &)            = delete;
    BinaryFileReader &operator=(const BinaryFileReader &) = delete;

    void verifyOpen() {
        ASSERT_EQ(file_.is_open(), true) << "Error: Could not open file " << filename_;
    }

    bool getLine(std::string &line) {
        line.clear();
        char curChar;
        bool hasData = false;

        if (!file_.is_open() || file_.eof()) {
            return false;
        }

        while (file_.get(curChar)) {
            if (curChar == '\r') {
                if (file_.peek() == '\n') {
                    file_.get();
                    return true;
                } else {
                    line    += curChar;
                    hasData  = true;
                }
            } else if (curChar == '\n') {
                return true;
            } else {
                line    += curChar;
                hasData  = true;
            }
        }

        return hasData;
    }

    [[nodiscard]]
    bool isOpen() const {
        return file_.is_open();
    }

private:
    // Use fprintf for non-throwing diagnostics inside this noexcept helper.
    static void closeStreamNoThrow(std::ifstream &fileStream) noexcept {
        try {
            fileStream.close();
        } catch (const std::exception &exception) {
            std::fprintf(stderr, "Exception in destructor: %s\n", exception.what());
        } catch (...) {
            std::fprintf(stderr, "Unknown exception in destructor\n");
        }
    }

    const fs::path filename_;
    std::ifstream  file_;
};

class LoggerTest : public mdn::GTestExtension {
protected:
    static inline mdn_Logger_StreamConfig_t streamConfigDefault;

    static inline std::regex regexFormatForFile;
    static inline std::regex regexFormatForScreen;

    static inline std::vector<std::regex>                                     loggingFormatToRegexMap;
    static inline std::vector<FormatRegexIndices>                             loggingFormatToIndicesMap;
    static constexpr std::array<const char *, MDN_LOGGER_LOGGING_LEVEL_COUNT> logLevelToStringMap = {
        {"DEBUG",
         "INFO",
         "WARNING",
         "ERROR",
         "CRITICAL"}
    };

    static constexpr std::array<const char *, MDN_LOGGER_LOGGING_LEVEL_COUNT> logLevelToAnsiColorMap = {
        {TEST_ANSI_COLOR("90"),
         TEST_ANSI_COLOR("0"),
         TEST_ANSI_COLOR("33"),
         TEST_ANSI_COLOR("31"),
         TEST_ANSI_COLOR("35")}
    };

    static constexpr const char *ansiResetColor = TEST_ANSI_COLOR("0");

    std::chrono::system_clock::time_point timePointPrev, timePointCur;

    struct OutputFileInfo {
        mdn_Logger_StreamConfig_t streamConfig;
        FILE                     *fileToRead;
        const std::string         suffix;
        std::string               path;
    };

    struct LogLine {
        mdn_Logger_loggingLevel_t loggingLevel;
        std::string               message;
    };

    std::array<OutputFileInfo, static_cast<std::size_t>(OutputFiles::COUNT)> outputFilesInfo{
        OutputFileInfo{.streamConfig = mdn_Logger_StreamConfig_t{nullptr, MDN_LOGGER_LOGGING_LEVEL_DEBUG, MDN_LOGGER_LOGGING_FORMAT_FILE},  .fileToRead = nullptr, .suffix = "logger1",           .path = ""},
        OutputFileInfo{.streamConfig = mdn_Logger_StreamConfig_t{nullptr, MDN_LOGGER_LOGGING_LEVEL_DEBUG, MDN_LOGGER_LOGGING_FORMAT_FILE},  .fileToRead = nullptr, .suffix = "logger2",           .path = ""},
        OutputFileInfo{.streamConfig = mdn_Logger_StreamConfig_t{stdout, MDN_LOGGER_LOGGING_LEVEL_DEBUG, MDN_LOGGER_LOGGING_FORMAT_SCREEN}, .fileToRead = nullptr, .suffix = "stdoutRedirection", .path = ""},
        OutputFileInfo{.streamConfig = mdn_Logger_StreamConfig_t{stderr, MDN_LOGGER_LOGGING_LEVEL_DEBUG, MDN_LOGGER_LOGGING_FORMAT_SCREEN}, .fileToRead = nullptr, .suffix = "stderrRedirection", .path = ""},
    };

    void logDebug(const std::string &message) {
        MDN_LOGGER_LOG_DEBUG(message.c_str());  // NOLINT(hicpp-vararg)
    }

    void logInfo(const std::string &message) {
        MDN_LOGGER_LOG_INFO(message.c_str());  // NOLINT(hicpp-vararg)
    }

    void logWarning(const std::string &message) {
        MDN_LOGGER_LOG_WARNING(message.c_str());  // NOLINT(hicpp-vararg)
    }

    void logError(const std::string &message) {
        MDN_LOGGER_LOG_ERROR(message.c_str());  // NOLINT(hicpp-vararg)
    }

    void logCritical(const std::string &message) {
        MDN_LOGGER_LOG_CRITICAL(message.c_str());  // NOLINT(hicpp-vararg)
    }

    using LogFuncCallback = void (LoggerTest::*)(const std::string &);
    std::array<LogFuncCallback, MDN_LOGGER_LOGGING_LEVEL_COUNT> logFunctions{
        {
         &LoggerTest::logDebug,
         &LoggerTest::logInfo,
         &LoggerTest::logWarning,
         &LoggerTest::logError,
         &LoggerTest::logCritical,
         }
    };

    void SetUp() override {
        mWMock = std::make_unique<MWMock>();
        MWMock::SetUp();

        initTestFullName();
    }

    void TearDown() override {
        mWMock.reset(nullptr);
    }

    void redirectRequiredStreamsStart(const std::vector<OutputFiles> &outputFiles) {
        for (const auto outputFile : outputFiles) {
            if (outputFile == OutputFiles::STDOUT_REDIRECTION) {
                ASSERT_EQ(mdn_StandardStreamsRedirection_start(MDN_STANDARD_STREAMS_REDIRECTION_STREAM_ID_STDOUT, outputFilesInfo[static_cast<std::size_t>(outputFile)].fileToRead), MDN_STATUS_SUCCESS);
            } else if (outputFile == OutputFiles::STDERR_REDIRECTION) {
                ASSERT_EQ(mdn_StandardStreamsRedirection_start(MDN_STANDARD_STREAMS_REDIRECTION_STREAM_ID_STDERR, outputFilesInfo[static_cast<std::size_t>(outputFile)].fileToRead), MDN_STATUS_SUCCESS);
            }
        }
    }

    static void redirectRequiredStreamsStop(const std::vector<OutputFiles> &outputFiles) {
        for (const auto outputFile : outputFiles) {
            if (outputFile == OutputFiles::STDOUT_REDIRECTION) {
                ASSERT_EQ(mdn_StandardStreamsRedirection_stop(MDN_STANDARD_STREAMS_REDIRECTION_STREAM_ID_STDOUT), MDN_STATUS_SUCCESS);
            } else if (outputFile == OutputFiles::STDERR_REDIRECTION) {
                ASSERT_EQ(mdn_StandardStreamsRedirection_stop(MDN_STANDARD_STREAMS_REDIRECTION_STREAM_ID_STDERR), MDN_STATUS_SUCCESS);
            }
        }
    }

    void printAllToLogs(const std::vector<LogLine> &logLines, const std::vector<OutputFiles> &outputFiles) {
        ASSERT_NO_FATAL_FAILURE(redirectRequiredStreamsStart(outputFiles));
        for (const auto &logLine : logLines) {
            (this->*logFunctions[logLine.loggingLevel])(logLine.message);
        }
        ASSERT_NO_FATAL_FAILURE(redirectRequiredStreamsStop(outputFiles));
    }

    void openTestOutputFiles(const std::vector<OutputFiles> &outputFiles) {
        fs::path testOutputFilesPath;

        testOutputFilesPath = testOutputDirPath / testFullName;

        for (const auto outputFile : outputFiles) {
            auto &outputFileRef       = outputFilesInfo[static_cast<std::size_t>(outputFile)];
            outputFileRef.path        = testOutputFilesPath.string();
            outputFileRef.path       += "_";
            outputFileRef.path       += outputFileRef.suffix;
            outputFileRef.path       += ".log";
            outputFileRef.fileToRead  = fopen(outputFileRef.path.c_str(), "w");
            ASSERT_NE(outputFileRef.fileToRead, nullptr)
                << "Failed to open file for writing: " << outputFileRef.path << "\n";
            if (outputFileRef.streamConfig.stream == nullptr) {
                outputFileRef.streamConfig.stream = outputFileRef.fileToRead;
            }
        }
    }

    void closeTestOutputFiles(const std::vector<OutputFiles> &outputFiles) {
        for (const auto outputFile : outputFiles) {
            const auto &outputFileRef = outputFilesInfo[static_cast<std::size_t>(outputFile)];
            if (fclose(outputFileRef.fileToRead) != 0) {
                FAIL() << "Failed to close file: " << outputFileRef.path;
            }
        }
    }

    void addOutputStreams(const std::vector<OutputFiles> &outputFiles) {
        for (const auto outputFile : outputFiles) {
            ASSERT_EQ(mdn_Logger_addOutputStream(outputFilesInfo[static_cast<std::size_t>(outputFile)].streamConfig), MDN_STATUS_SUCCESS);
        }
    }

    void verifyTimestamp(const std::smatch &matches, mdn_Logger_loggingFormat_t loggingFormat) {
        const auto &indices = loggingFormatToIndicesMap[loggingFormat];
        std::string time;
        std::string date;
        struct tm   parsedLocalTime = {};
        int         milliseconds;

        time = matches[indices.timeIndex].str();
        if (sscanf(time.c_str(), "%d:%d:%d.%d", &parsedLocalTime.tm_hour, &parsedLocalTime.tm_min, &parsedLocalTime.tm_sec, &milliseconds) != 4) {  // NOLINT(cert-err34-c,hicpp-vararg)
            FAIL() << "Failed to parse time: " << time;
        }

        if (indices.dateIndex.has_value()) {
            date = matches[indices.dateIndex.value()].str();
            if (sscanf(date.c_str(), "%d-%d-%d", &parsedLocalTime.tm_year, &parsedLocalTime.tm_mon, &parsedLocalTime.tm_mday) != 3) {  // NOLINT(cert-err34-c,hicpp-vararg)
                FAIL() << "Failed to parse date: " << date;
            }
            parsedLocalTime.tm_year -= YEAR_OFFSET;   // Years since 1900
            parsedLocalTime.tm_mon  -= MONTH_OFFSET;  // Months are 0-11
        } else {
            auto      now           = std::chrono::system_clock::now();
            auto      time_t_now    = std::chrono::system_clock::to_time_t(now);
            struct tm currentTm     = safeLocalTime(time_t_now);
            parsedLocalTime.tm_year = currentTm.tm_year;
            parsedLocalTime.tm_mon  = currentTm.tm_mon;
            parsedLocalTime.tm_mday = currentTm.tm_mday;
        }

        auto parsedTimePoint = std::chrono::system_clock::from_time_t(std::mktime(&parsedLocalTime));
        timePointCur         = parsedTimePoint + std::chrono::milliseconds(milliseconds);

        // Note: Very rare edge case - tests running at midnight might show
        // "time going backwards" for screen format due to day rollover
        if (timePointCur < timePointPrev) {
            if (indices.dateIndex.has_value()) {
                FAIL() << "Log timestamp go back in time: " << date << " " << time;
            } else {
                FAIL() << "Log timestamp go back in time: " << time;
            }
        }

        timePointPrev = timePointCur;
    }

    static void verifyLogLevel(const std::smatch &matches, mdn_Logger_loggingFormat_t loggingFormat, const LogLine &expectedLogLine, const std::string &actualLogLine) {
        const auto &indices = loggingFormatToIndicesMap[loggingFormat];

        if (indices.logLevelIndex.has_value()) {
            auto        actualLogLevel   = matches[indices.logLevelIndex.value()].str();
            const auto &expectedLogLevel = logLevelToStringMap[expectedLogLine.loggingLevel];
            ASSERT_EQ(actualLogLevel, expectedLogLevel) << "Log level mismatch in line: " << actualLogLine;
        } else if (indices.colorPrefixIndex.has_value() && indices.colorSuffixIndex.has_value()) {
            auto actualColorPrefix = matches[indices.colorPrefixIndex.value()].str();
            auto actualColorSuffix = matches[indices.colorSuffixIndex.value()].str();

            const auto &expectedColorPrefix = logLevelToAnsiColorMap[expectedLogLine.loggingLevel];
            ASSERT_EQ(actualColorPrefix, expectedColorPrefix) << "Color prefix mismatch in line: " << actualLogLine;
            ASSERT_EQ(actualColorSuffix, ansiResetColor) << "Color suffix should be reset code in line: " << actualLogLine;
        }
    }

    void verifyFunctionName(const std::smatch &matches, mdn_Logger_loggingFormat_t loggingFormat, const std::string &actualLogLine) {
        const auto &indices = loggingFormatToIndicesMap[loggingFormat];

        auto actualFunction   = matches[indices.functionIndex].str();
        auto expectedFunction = testFullName;
        ASSERT_EQ(actualFunction, expectedFunction) << "Function name mismatch in line: " << actualLogLine;
    }

    static void verifyMessage(const std::smatch &matches, mdn_Logger_loggingFormat_t loggingFormat, const LogLine &expectedLogLine, const std::string &actualLogLine) {
        const auto &indices = loggingFormatToIndicesMap[loggingFormat];

        auto actualMessage = matches[indices.messageIndex].str();
        ASSERT_EQ(actualMessage, expectedLogLine.message) << "Message content mismatch in line: " << actualLogLine;
    }

    void verifyLogLine(const std::string &actualLogLine, mdn_Logger_loggingFormat_t loggingFormat, const LogLine &expectedLogLine) {
        std::smatch matches;
        std::regex  format;

        format = loggingFormatToRegexMap[loggingFormat];

        ASSERT_EQ(std::regex_match(actualLogLine, matches, format), true) << "Line format isn't valid:\n"
                                                                          << actualLogLine;

        ASSERT_NO_FATAL_FAILURE(verifyTimestamp(matches, loggingFormat));
        ASSERT_NO_FATAL_FAILURE(verifyLogLevel(matches, loggingFormat, expectedLogLine, actualLogLine));
        ASSERT_NO_FATAL_FAILURE(verifyFunctionName(matches, loggingFormat, actualLogLine));
        ASSERT_NO_FATAL_FAILURE(verifyMessage(matches, loggingFormat, expectedLogLine, actualLogLine));
    }

    void verifyLogLinesForLogFile(BinaryFileReader &binaryFileReader, const OutputFileInfo &outputFileRef, const std::vector<LogLine> &logLines) {
        for (const auto &expectedLogLine : logLines) {
            if (expectedLogLine.loggingLevel < outputFileRef.streamConfig.loggingLevel) {
                continue;
            }
            std::string actualLogLine;
            ASSERT_EQ(binaryFileReader.getLine(actualLogLine), true) << "Failed to read line from file: " << outputFileRef.path;
            ASSERT_NO_FATAL_FAILURE(verifyLogLine(actualLogLine, outputFileRef.streamConfig.loggingFormat, expectedLogLine));
        }
    }

    void verifyLogFiles(const std::vector<LogLine> &logLines, const std::vector<OutputFiles> &outputFiles) {
        for (const auto outputFile : outputFiles) {
            auto &outputFileRef    = outputFilesInfo[static_cast<std::size_t>(outputFile)];
            auto  binaryFileReader = BinaryFileReader(outputFileRef.path);
            ASSERT_NO_FATAL_FAILURE(binaryFileReader.verifyOpen());
            timePointPrev = std::chrono::system_clock::time_point{};
            ASSERT_NO_FATAL_FAILURE(verifyLogLinesForLogFile(binaryFileReader, outputFileRef, logLines));
        }
    }

public:
    static void SetUpTestSuite() {
        initTestSuitePaths();

        if (!fs::exists(testOutputDirPath)) {
            fs::create_directories(testOutputDirPath);
        }

        streamConfigDefault = {
            .stream        = stdout,
            .loggingLevel  = MDN_LOGGER_LOGGING_LEVEL_DEBUG,
            .loggingFormat = MDN_LOGGER_LOGGING_FORMAT_SCREEN};

        loggingFormatToRegexMap.resize(MDN_LOGGER_LOGGING_FORMAT_COUNT);
        loggingFormatToRegexMap[MDN_LOGGER_LOGGING_FORMAT_FILE]   = R"(^(\d{4}-\d{2}-\d{2}) (\d{2}:\d{2}:\d{2}\.\d{3}) (\w+) +([\w\.]+) \| ([[:print:]\s]+)$)";
        loggingFormatToRegexMap[MDN_LOGGER_LOGGING_FORMAT_SCREEN] = R"(^(\x1B\[\d+m)(\d{2}:\d{2}:\d{2}\.\d{3}) ([\w\.]+) \| ([[:print:]\s]+)(\x1B\[\d+m)$)";

        loggingFormatToIndicesMap.resize(MDN_LOGGER_LOGGING_FORMAT_COUNT);

        // FILE format: (date) (time) (level) (function) | (message)
        loggingFormatToIndicesMap[MDN_LOGGER_LOGGING_FORMAT_FILE] = {
            .dateIndex        = 1,
            .timeIndex        = 2,
            .logLevelIndex    = 3,
            .functionIndex    = 4,
            .messageIndex     = 5,
            .colorPrefixIndex = std::nullopt,
            .colorSuffixIndex = std::nullopt};

        // SCREEN format: (color_prefix) (time) (function) | (message) (color_suffix)
        loggingFormatToIndicesMap[MDN_LOGGER_LOGGING_FORMAT_SCREEN] = {
            .dateIndex        = std::nullopt,
            .timeIndex        = 2,
            .logLevelIndex    = std::nullopt,
            .functionIndex    = 3,
            .messageIndex     = 4,
            .colorPrefixIndex = 1,
            .colorSuffixIndex = 5};
    }
};

TEST_F(LoggerTest, InitAndDeinit) {
    ASSERT_EQ(mdn_Logger_init(), MDN_STATUS_SUCCESS);
    ASSERT_EQ(mdn_Logger_deinit(), MDN_STATUS_SUCCESS);
}

TEST_F(LoggerTest, AddOutputStream) {
    ASSERT_EQ(mdn_Logger_init(), MDN_STATUS_SUCCESS);
    ASSERT_EQ(mdn_Logger_addOutputStream(streamConfigDefault), MDN_STATUS_SUCCESS);
    ASSERT_EQ(mdn_Logger_deinit(), MDN_STATUS_SUCCESS);
}

TEST_F(LoggerTest, PrintToFile) {
    std::vector<OutputFiles> outputFiles = {OutputFiles::LOGGER_OUTPUT_1, OutputFiles::STDOUT_REDIRECTION};
    std::vector<LogLine>     logLines    = {
        LogLine{MDN_LOGGER_LOGGING_LEVEL_DEBUG,    "Grey debug message"     },
        LogLine{MDN_LOGGER_LOGGING_LEVEL_INFO,     "White info message"     },
        LogLine{MDN_LOGGER_LOGGING_LEVEL_WARNING,  "Yellow warning message" },
        LogLine{MDN_LOGGER_LOGGING_LEVEL_ERROR,    "Red error message"      },
        LogLine{MDN_LOGGER_LOGGING_LEVEL_CRITICAL, "Purple critical message"},
    };

    ASSERT_NO_FATAL_FAILURE(openTestOutputFiles(outputFiles));
    ASSERT_EQ(mdn_Logger_init(), MDN_STATUS_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(addOutputStreams(outputFiles));
    ASSERT_NO_FATAL_FAILURE(printAllToLogs(logLines, outputFiles));
    ASSERT_EQ(mdn_Logger_deinit(), MDN_STATUS_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(closeTestOutputFiles(outputFiles));

    ASSERT_NO_FATAL_FAILURE(verifyLogFiles(logLines, outputFiles));
}

#ifdef MDN_LOGGER_SAFE_MODE

class LoggerSafeModeTest : public ::LoggerTest {
protected:
    static inline mdn_Logger_StreamConfig_t streamConfigNullStream;
    static inline mdn_Logger_StreamConfig_t streamConfigLoggingLevelTooBig;
    static inline mdn_Logger_StreamConfig_t streamConfigLoggingLevelTooSmall;
    static inline mdn_Logger_StreamConfig_t streamConfigLoggingFormatTooBig;
    static inline mdn_Logger_StreamConfig_t streamConfigLoggingFormatTooSmall;

public:
    static void SetUpTestSuite() {
        streamConfigNullStream        = streamConfigDefault;
        streamConfigNullStream.stream = nullptr;

        streamConfigLoggingLevelTooBig              = streamConfigDefault;
        streamConfigLoggingLevelTooBig.loggingLevel = MDN_LOGGER_LOGGING_LEVEL_COUNT;

        streamConfigLoggingLevelTooSmall              = streamConfigDefault;
        streamConfigLoggingLevelTooSmall.loggingLevel = static_cast<mdn_Logger_loggingLevel_t>(-1);

        streamConfigLoggingFormatTooBig               = streamConfigDefault;
        streamConfigLoggingFormatTooBig.loggingFormat = MDN_LOGGER_LOGGING_FORMAT_COUNT;

        streamConfigLoggingFormatTooSmall               = streamConfigDefault;
        streamConfigLoggingFormatTooSmall.loggingFormat = static_cast<mdn_Logger_loggingFormat_t>(-1);
    }
};

TEST_F(LoggerSafeModeTest, InvalidArguments) {
    ASSERT_EQ(mdn_Logger_addOutputStream(streamConfigDefault), MDN_STATUS_ERROR_LIBRARY_NOT_INITIALIZED);

    ASSERT_EQ(mdn_Logger_init(), MDN_STATUS_SUCCESS);
    ASSERT_EQ(mdn_Logger_init(), MDN_STATUS_ERROR_LIBRARY_ALREADY_INITIALIZED);

    ASSERT_EQ(mdn_Logger_addOutputStream(streamConfigNullStream), MDN_STATUS_ERROR_BAD_ARGUMENT);
    ASSERT_EQ(mdn_Logger_addOutputStream(streamConfigLoggingLevelTooBig), MDN_STATUS_ERROR_BAD_ARGUMENT);
    ASSERT_EQ(mdn_Logger_addOutputStream(streamConfigLoggingLevelTooSmall), MDN_STATUS_ERROR_BAD_ARGUMENT);
    ASSERT_EQ(mdn_Logger_addOutputStream(streamConfigLoggingFormatTooBig), MDN_STATUS_ERROR_BAD_ARGUMENT);
    ASSERT_EQ(mdn_Logger_addOutputStream(streamConfigLoggingFormatTooSmall), MDN_STATUS_ERROR_BAD_ARGUMENT);

    ASSERT_EQ(mdn_Logger_deinit(), MDN_STATUS_SUCCESS);
    ASSERT_EQ(mdn_Logger_deinit(), MDN_STATUS_ERROR_LIBRARY_NOT_INITIALIZED);
}

#endif  // MDN_LOGGER_SAFE_MODE

#ifdef MDN_MW_ENABLE_MOCKING

class LoggerTestMemoryAllocationFailure : public LoggerTest {};

TEST_F(LoggerTestMemoryAllocationFailure, InitFail) {
    EXPECT_CALL(*mWMock, malloc(StrEq("mdn_Logger_init"), _))
        .WillOnce(Return(nullptr));

    ASSERT_EQ(mdn_Logger_init(), MDN_STATUS_ERROR_MEM_ALLOC);
}

TEST_F(LoggerTestMemoryAllocationFailure, AddOutputStreamFail) {
    EXPECT_CALL(*mWMock, realloc(StrEq("mdn_Logger_addOutputStream"), _, _))
        .WillOnce(Return(nullptr));

    ASSERT_EQ(mdn_Logger_init(), MDN_STATUS_SUCCESS);
    ASSERT_EQ(mdn_Logger_addOutputStream(streamConfigDefault), MDN_STATUS_ERROR_MEM_ALLOC);
    ASSERT_EQ(mdn_Logger_deinit(), MDN_STATUS_SUCCESS);
}

#endif  // MDN_MW_ENABLE_MOCKING

int main(int argc, char *argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
