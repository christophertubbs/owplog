#include <stdexcept>
#include <algorithm>
#include <deque>
#include <random>
#include <iostream>
#include <filesystem>

#include "owplog/owplog.h"
#include "c_component.h"
#include "cpp_component.hpp"
#include "threadpool.cpp"

#if defined(__linux__)
    #include <unistd.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
#else
        #error "Unsupported platform only __linux__ and __APPLE__ are supported"
#endif

std::filesystem::path get_executable_path() {
    #if defined(__linux__)
    std::size_t path_buffer_size = 1024;

    while (true) {
        std::vector<char> path_buffer(path_buffer_size, '\0');
        const ssize_t character_count = readlink(
            "/proc/self/exe",
            path_buffer.data(),
            path_buffer.size()
        );
        if (character_count < 0) {
            throw std::runtime_error("Failed to read /proc/self/exe");
        }
        auto resolved_character_count = static_cast<std::size_t>(character_count);
        if (resolved_character_count < path_buffer_size) {
            return {std::string(path_buffer.data(), resolved_character_count)};
        }
        path_buffer_size *= 2;
    }
    #else
    uint32_t path_buffer_size = 0;

    _NSGetExecutablePath(nullptr, &path_buffer_size);

    std::vector<char> path_buffer(path_buffer_size, '\0');

    const int result = _NSGetExecutablePath(path_buffer.data(), &path_buffer_size);

    if (result != 0) {
        throw std::runtime_error("Failed to get executable path");
    }

    return std::filesystem::path(path_buffer.data());
    #endif
}

std::filesystem::path get_executable_directory() {
    return get_executable_path().parent_path();
}


#if defined(OWPLOG_E2E_HAVE_FORTRAN)
extern "C" void fortran_component_run_fake_model(int iteration_count, int basin_count, int rank_value);

void do_fortran_work() {
    fortran_component_run_fake_model(10, 3, -1);
}
#endif

#if defined(OWPLOG_E2E_HAVE_PYTHON_EMBED)
#include <pybind11/embed.h>
#include <string>
#include <vector>

namespace py = pybind11;

const std::string DEFAULT_PYTHON_MODULE_NAME = "python_consumer";
const std::string DEFAULT_PYTHON_FUNCTION_NAME = "main";

class PythonScriptRunner {
    public:
        static std::function<void()> create_callable(
            const std::string& module_name = DEFAULT_PYTHON_MODULE_NAME,
            const std::string& function_name = DEFAULT_PYTHON_FUNCTION_NAME
        ) {
            auto runner = std::make_shared<PythonScriptRunner>(module_name, function_name);
            return [runner]() {
                runner->run();
            };
        }
        explicit PythonScriptRunner(
            const std::string& module_name = DEFAULT_PYTHON_MODULE_NAME,
            const std::string& function_name = DEFAULT_PYTHON_FUNCTION_NAME
        ) {
            py::gil_scoped_acquire acquire;
            this->module_ = py::module_::import(module_name.c_str());
            this->function_ = this->module_.attr(function_name.c_str());
        }

        ~PythonScriptRunner() {
            this->close();
        }

        void close() {
            if (Py_IsInitialized()) {
                this->function_ = py::object{};
                this->module_ = py::object{};
            } else {
                // The interpreter is already gone. Releasing avoids py::object destructors trying to DECREF.
                // This leaks resources, but its better than crashing
                static_cast<void>(this->function_.release());
                static_cast<void>(this->module_.release());
            }
        }
        void run() const {
            py::gil_scoped_acquire acquired_gil{};
            py::object result = this->function_();
        }

        void operator()() const {
            this->run();
        }
    private:
        py::object module_;
        py::object function_;
};

PYBIND11_EMBEDDED_MODULE(owplog_native_bridge, m) {
    m.def(
        "emit",
        [](
            const std::string& logger_name,
            const std::string& message,
            const std::string& file_name,
            const int line_no,
            const std::string& function_name,
            const int rank
        ) {
            const owplog::SourceLocation source{file_name, line_no, function_name};
            owplog::Logger::log(
                owplog::LogLevel::Info,
                message,
                logger_name,
                source,
                owplog::LogOrigin::Application,
                rank,
                "python");
        }
    );
}

void call_loose_python() {
    py::gil_scoped_acquire acquire;
    py::exec(R"PY(
import inspect
import owplog_native_bridge

def python_component():
    f = inspect.currentframe()
    owplog_native_bridge.emit(
        "e2e.python",
        "message from Python embedded in native e2e",
        f.f_code.co_filename,
        f.f_lineno,
        f.f_code.co_name,
        -1,
    )

python_component()
)PY");
}
#endif

const std::string LOG_PATTERN = "[%(asctime)s] %(levelname)s %(name)s %(relativeCreated)dms Elapsed %(filename)s::%(funcName)s#%(lineno)d - %(message)s";
constexpr owplog::LogLevel LOG_LEVEL = owplog::LogLevel::Trace;
constexpr size_t JOB_COUNT_PER_TYPE = 1000;
const std::string APPLICATION_NAME = "End2End";
const std::filesystem::path ROOT_PATH = std::filesystem::current_path();
const std::filesystem::path LOG_PATH = ROOT_PATH / (APPLICATION_NAME + ".log");
constexpr bool WRITE_TO_CONSOLE = true;
constexpr bool APPEND_TO_LOG = false;

inline void setup_logging() {
    owplog::LoggerConfigurationArguments configuration{
        .format = LOG_PATTERN,
        .level = LOG_LEVEL
    };
    configuration.handlers = std::vector<owplog::handlers::LogHandlerPtr>{
        std::make_shared<owplog::handlers::FileHandler>(LOG_PATH, APPEND_TO_LOG)
    };

    if (WRITE_TO_CONSOLE) {
        configuration.handlers->push_back(std::make_shared<owplog::handlers::ConsoleHandler>());
    }
    owplog::basicConfig(configuration);
    OWPLOG_LOG(owplog::LogLevel::Info, APPLICATION_NAME, "main started and logger configured");
}

template<typename DurationType = std::chrono::milliseconds>
void await_jobs(std::deque<std::future<void>>& futures, const DurationType wait_time = std::chrono::milliseconds(500)) {
    int completed = 0;
    int not_ready_count = 0;

    std::string message = std::string("Waiting for ") + std::to_string(futures.size()) + " jobs to complete";
    OWPLOG_LOG(owplog::LogLevel::Info, APPLICATION_NAME, message);

    while (!futures.empty()) {
        std::future<void> future = std::move(futures.front());
        futures.pop_front();

        std::future_status status = future.wait_for(wait_time);

        if (status == std::future_status::ready) {
            ++completed;
            try {
                future.get();
            } catch (const std::exception& e) {
                OWPLOG_LOG_ERROR_MSG(std::string("Exception in job: ") + e.what());
            }
            if (completed % 100 == 0) {
                message = std::string("Completed ") + std::to_string(completed) + " jobs";
                OWPLOG_LOG(owplog::LogLevel::Debug, APPLICATION_NAME, message);
            }
        } else {
            futures.push_back(std::move(future));
            ++not_ready_count;

            if (not_ready_count >= 500) {
                std::cerr << "Waited for jobs to finish at least 500 times and they are still going. " << futures.size() << " jobs remaining" << std::endl;
                not_ready_count = 0;
            }
        }
    }

    message = std::string("Completed ") + std::to_string(completed) + " jobs";
    OWPLOG_LOG(owplog::LogLevel::Info, APPLICATION_NAME, message);
}

auto organize_jobs(const std::vector<std::function<void()>>& available_jobs) -> std::vector<std::function<void()>> {
    std::vector<std::function<void()>> jobs_to_schedule;
    jobs_to_schedule.reserve(available_jobs.size() * JOB_COUNT_PER_TYPE);

    for (auto& available_job: available_jobs) {
        for (size_t i = 0; i < JOB_COUNT_PER_TYPE; ++i) {
            jobs_to_schedule.push_back(available_job);
        }
    }

    std::mt19937 generator(std::random_device{}());
    std::shuffle(jobs_to_schedule.begin(), jobs_to_schedule.end(), generator);
    return jobs_to_schedule;
}

int main() {
    setup_logging();
    ThreadPool pool = ThreadPool(5);

    std::vector<std::function<void()>> available_jobs;
    available_jobs.reserve(8);

    available_jobs.emplace_back(cpp_component_emit_logs);
    available_jobs.emplace_back(c_component_emit_logs);
    available_jobs.emplace_back(cpp_echo_messages);

#if defined(OWPLOG_E2E_HAVE_FORTRAN)
    available_jobs.emplace_back(do_fortran_work);
#else
    std::cout << "[INFO] Fortran compiler not detected; skipping Fortran component.\n";
#endif

#if defined(OWPLOG_E2E_HAVE_PYTHON_EMBED)
    // Write to stdout to show that a console logger can still pick this up
    std::cout << "[INFO] Python embed dependencies detected; running Python component.\n";
    py::scoped_interpreter guard{};
    py::module_ sys = py::module::import("sys");
    sys.attr("path").attr("insert")(0, get_executable_directory().string());
    auto python_script_runner = PythonScriptRunner::create_callable();
    available_jobs.emplace_back(python_script_runner);
    available_jobs.emplace_back(call_loose_python);
    py::gil_scoped_release release{};
#else
    std::cout << "[INFO] Python embed dependencies not detected; skipping Python component.\n";
#endif
    std::vector<std::function<void()>> jobs_to_schedule = organize_jobs(available_jobs);

    std::deque<std::future<void>> futures;

    for (auto& job: jobs_to_schedule) {
        futures.push_back(pool.submit(job));
    }

    await_jobs(futures);
    owplog::Logger::log(owplog::LogLevel::Info, "main finished", APPLICATION_NAME, OWPLOG_SOURCE_LOCATION);
    return 0;
}
