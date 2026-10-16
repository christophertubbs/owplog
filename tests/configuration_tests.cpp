#include "owplog/configuration.hpp"
#include <gtest/gtest.h>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <unistd.h>

namespace {
class MemoryHandler : public owplog::handlers::LogHandler {
public:
    std::string text;
    void write(const owplog::LogRecord&, const std::string& formatted) override { text += formatted; }
};
using namespace owplog::configuration;
TEST(Configuration, CustomFactoriesAndOrderedInstancesReachLogger) {
    HandlerRegistry registry;
    std::vector<std::shared_ptr<MemoryHandler>> created;
    registry.register_handler("library.Memory", [&](const HandlerOptions& options) {
        EXPECT_EQ(options.at("tag"), created.empty() ? "second" : "first");
        auto handler = std::make_shared<MemoryHandler>();
        created.push_back(handler);
        return handler;
    });
    std::istringstream input("[logger]\nhandlers=b,a\nlevel=debug\nformat=%(message)s\nforce=true\n"
                             "[handler.a]\nclass=library.Memory\ntag=first\n"
                             "[handler.b]\nclass=library.Memory\ntag=second\n");
    auto arguments = load(input, registry);
    ASSERT_EQ(arguments.handlers->size(), 2u);
    EXPECT_EQ(arguments.handlers->at(0), created.at(0));
    EXPECT_TRUE(arguments.force);
    EXPECT_EQ(arguments.level, owplog::LogLevel::Debug);
    owplog::core::Logger logger(arguments);
    logger.info("configured output");
    logger.shutdown();
    logger.flush();
    EXPECT_NE(created.at(0)->text.find("configured output"), std::string::npos);
    EXPECT_EQ(created.at(0)->text, created.at(1)->text);
}
TEST(Configuration, RejectsInvalidDocuments) {
    HandlerRegistry registry;
    for (const std::string document : {
        "", "[other]\nx=y", "[logger]\nlevel=bogus", "[logger]\nforce=yes",
        "[logger]\nlevel=info\nlevel=debug", "[logger]\n[logger]",
        "[logger]\nhandlers=", "[logger]\nhandlers=a,", "[logger]\nhandlers=a,a",
        "[logger]\nhandlers=missing", "[logger]\nhandlers=a\nfilename=x",
        "[logger]\nunknown=x", "[logger]\n[handler.a]\nclass=Missing",
        "[logger]\nhandlers=a\n[handler.a]\nclass=FileHandler",
        "[logger]\nhandlers=a\n[handler.a]\nclass=ConsoleHandler\nwrong=true"}) {
        std::istringstream input(document);
        EXPECT_THROW(load(input, registry), ConfigurationError) << document;
    }
}
TEST(Configuration, ResolvesReferencesBeforeFactories) {
    HandlerRegistry registry;
    int calls = 0;
    registry.register_handler("custom", [&](const HandlerOptions&) {
        ++calls;
        return std::make_shared<MemoryHandler>();
    });
    std::istringstream input("[logger]\nhandlers=a,missing\n[handler.a]\nclass=custom");
    EXPECT_THROW(load(input, registry), ConfigurationError);
    EXPECT_EQ(calls, 0);
}
TEST(Configuration, FactoryFailuresAndRegistration) {
    HandlerRegistry registry;
    EXPECT_THROW(registry.register_handler("ConsoleHandler", [](const HandlerOptions&) { return nullptr; }), ConfigurationError);
    EXPECT_THROW(registry.register_handler("", HandlerFactory{}), ConfigurationError);
    registry.register_handler("null", [](const HandlerOptions&) { return nullptr; });
    registry.register_handler("throws", [](const HandlerOptions&) -> owplog::handlers::LogHandlerPtr { throw std::runtime_error("failure"); });
    for (const std::string name : {"null", "throws"}) {
        std::istringstream input("[logger]\nhandlers=a\n[handler.a]\nclass=" + name);
        EXPECT_THROW(load(input, registry), ConfigurationError);
    }
}
TEST(Configuration, FileLoadingPreservesLiteralValuesAndWritesOutput) {
    HandlerRegistry registry;
    const std::string base = (std::filesystem::temp_directory_path() / ("owplog_config_" + std::to_string(getpid()))).string();
    const std::string configuration_path = base + ".ini";
    const std::string output_path = base + ".log";
    {
        std::ofstream file(configuration_path);
        file << "# comment\n[logger]\nhandlers=disk\nformat=%(message)s # literal=value\n"
             << "datefmt=%Y\n[handler.disk]\nclass=FileHandler\nappend=false\npath=" << output_path << '\n';
    }
    {
        auto arguments = load_file(configuration_path, registry);
        EXPECT_EQ(arguments.format, "%(message)s # literal=value");
        EXPECT_EQ(arguments.datefmt, "%Y");
        owplog::core::Logger logger(arguments);
        logger.info("file configured");
        logger.shutdown();
    logger.flush();
    }
    std::ifstream output(output_path);
    std::string line;
    std::getline(output, line);
    EXPECT_NE(line.find("file configured # literal=value"), std::string::npos);
    std::filesystem::remove(configuration_path);
    std::filesystem::remove(output_path);
    EXPECT_THROW(load_file(configuration_path, registry), ConfigurationError);
}
TEST(Configuration, StreamFailureAndFilenameShorthand) {
    HandlerRegistry registry;
    std::istringstream broken("[logger]");
    broken.setstate(std::ios::badbit);
    EXPECT_THROW(load(broken, registry), ConfigurationError);
    std::istringstream input("[logger]\nfilename=run.log\nlevel=OFF\n");
    auto arguments = load(input, registry);
    EXPECT_EQ(arguments.filename, "run.log");
    EXPECT_FALSE(arguments.handlers.has_value());
    EXPECT_EQ(arguments.level, owplog::LogLevel::Off);
}
}
