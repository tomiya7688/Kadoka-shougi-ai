#include "kadoka/runtime/process_ai_backend.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <ctime>
#include <poll.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace kadoka::shogi::runtime {
namespace {

std::string build_request(
    std::size_t request_id,
    const Position& position,
    const SearchLimits& limits) {
    std::ostringstream out;
    out << "request " << request_id << '\n';
    out << "sfen " << position.to_sfen() << '\n';
    out << "time_ms ";
    if (limits.time_limit.has_value()) {
        out << limits.time_limit->count();
    } else {
        out << -1;
    }
    out << '\n';

    out << "nodes ";
    if (limits.node_limit.has_value()) {
        out << *limits.node_limit;
    } else {
        out << -1;
    }
    out << '\n';

    out << "depth ";
    if (limits.depth_limit.has_value()) {
        out << *limits.depth_limit;
    } else {
        out << -1;
    }
    out << '\n';
    out << "end\n";
    return out.str();
}

Square checked_square(int file, int rank) {
    if (file < 1 || file > 9 || rank < 1 || rank > 9) {
        throw std::runtime_error("external AI response contains out-of-range square");
    }
    return Square{
        static_cast<std::uint8_t>(file),
        static_cast<std::uint8_t>(rank),
    };
}

PieceType parse_drop_piece(const std::string& code) {
    if (code == "P") return PieceType::Pawn;
    if (code == "L") return PieceType::Lance;
    if (code == "N") return PieceType::Knight;
    if (code == "S") return PieceType::Silver;
    if (code == "G") return PieceType::Gold;
    if (code == "B") return PieceType::Bishop;
    if (code == "R") return PieceType::Rook;
    throw std::runtime_error("external AI response contains invalid drop piece");
}

SearchResult parse_response(
    std::size_t expected_request_id,
    const std::vector<std::string>& lines) {
    if (lines.empty()) {
        throw std::runtime_error("external AI returned an empty response");
    }

    {
        std::istringstream first(lines.front());
        std::string kind;
        std::size_t request_id = 0;
        if (!(first >> kind >> request_id)
            || kind != "result"
            || request_id != expected_request_id) {
            throw std::runtime_error("external AI response has invalid request id");
        }
    }

    SearchResult result;
    bool has_move = false;

    for (std::size_t index = 1; index < lines.size(); ++index) {
        std::istringstream parser(lines[index]);
        std::string kind;
        parser >> kind;
        if (kind.empty()) continue;

        if (kind == "move") {
            if (has_move) {
                throw std::runtime_error("external AI response contains multiple moves");
            }

            std::string move_kind;
            if (!(parser >> move_kind)) {
                throw std::runtime_error("external AI response contains malformed move");
            }

            if (move_kind == "normal") {
                int from_file = 0;
                int from_rank = 0;
                int to_file = 0;
                int to_rank = 0;
                int promote = 0;
                if (!(parser >> from_file >> from_rank >> to_file >> to_rank >> promote)
                    || (promote != 0 && promote != 1)) {
                    throw std::runtime_error("external AI response contains malformed normal move");
                }
                result.best_move.from = checked_square(from_file, from_rank);
                result.best_move.to = checked_square(to_file, to_rank);
                result.best_move.drop_piece = PieceType::None;
                result.best_move.promote = promote != 0;
            } else if (move_kind == "drop") {
                std::string piece;
                int to_file = 0;
                int to_rank = 0;
                if (!(parser >> piece >> to_file >> to_rank)) {
                    throw std::runtime_error("external AI response contains malformed drop move");
                }
                result.best_move.from.reset();
                result.best_move.to = checked_square(to_file, to_rank);
                result.best_move.drop_piece = parse_drop_piece(piece);
                result.best_move.promote = false;
            } else {
                throw std::runtime_error("external AI response contains unknown move kind");
            }
            has_move = true;
        } else if (kind == "score_cp") {
            long long value = 0;
            if (!(parser >> value)
                || value < std::numeric_limits<std::int32_t>::min()
                || value > std::numeric_limits<std::int32_t>::max()) {
                throw std::runtime_error("external AI response contains malformed score_cp");
            }
            result.score_cp = static_cast<std::int32_t>(value);
        } else if (kind == "nodes") {
            std::uint64_t value = 0;
            if (!(parser >> value)) {
                throw std::runtime_error("external AI response contains malformed nodes");
            }
            result.nodes = value;
        } else if (kind == "depth") {
            unsigned value = 0;
            if (!(parser >> value)) {
                throw std::runtime_error("external AI response contains malformed depth");
            }
            result.depth = value;
        } else if (kind == "info") {
            std::getline(parser, result.info);
            if (!result.info.empty() && result.info.front() == ' ') {
                result.info.erase(result.info.begin());
            }
        } else {
            throw std::runtime_error("external AI response contains unknown record: " + kind);
        }
    }

    if (!has_move) {
        throw std::runtime_error("external AI response does not contain move");
    }
    return result;
}

#ifdef _WIN32

std::string quote_windows_arg(const std::string& value) {
    if (value.find_first_of(" \t\"") == std::string::npos) return value;

    std::string result = "\"";
    std::size_t backslashes = 0;
    for (const char ch : value) {
        if (ch == '\\') {
            ++backslashes;
            continue;
        }
        if (ch == '"') {
            result.append(backslashes * 2 + 1, '\\');
            result.push_back('"');
            backslashes = 0;
            continue;
        }
        result.append(backslashes, '\\');
        backslashes = 0;
        result.push_back(ch);
    }
    result.append(backslashes * 2, '\\');
    result.push_back('"');
    return result;
}

#else

ssize_t write_without_sigpipe(int fd, const void* data, std::size_t size) {
    sigset_t blocked{};
    sigset_t old_mask{};
    sigset_t pending{};
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGPIPE);

    const int mask_result = pthread_sigmask(SIG_BLOCK, &blocked, &old_mask);
    bool sigpipe_was_pending = false;
    if (mask_result == 0 && sigpending(&pending) == 0) {
        sigpipe_was_pending = sigismember(&pending, SIGPIPE) == 1;
    }

    const ssize_t result = ::write(fd, data, size);
    const int saved_errno = errno;

    if (result < 0 && saved_errno == EPIPE && mask_result == 0 && !sigpipe_was_pending) {
        timespec no_wait{};
        while (::sigtimedwait(&blocked, nullptr, &no_wait) < 0 && errno == EINTR) {
        }
    }

    if (mask_result == 0) {
        (void)pthread_sigmask(SIG_SETMASK, &old_mask, nullptr);
    }
    errno = saved_errno;
    return result;
}

#endif

} // namespace

class PersistentProcessAIBackend::Impl {
public:
    explicit Impl(PersistentProcessBackendConfig config)
        : config_(std::move(config)) {
        if (config_.executable.empty()) {
            throw std::invalid_argument("process AI executable must not be empty");
        }
        if (config_.kind != AIBackendKind::ExternalProcess
            && config_.kind != AIBackendKind::Script) {
            throw std::invalid_argument(
                "PersistentProcessAIBackend kind must be external_process or script"
            );
        }
        config_.arguments.push_back("--kadoka-session");
        start();
    }

    ~Impl() {
        stop();
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    SearchResult decide(
        std::size_t request_id,
        const Position& position,
        const SearchLimits& limits) {
        write_all(build_request(request_id, position, limits));
        return parse_response(request_id, read_response(request_id));
    }

private:
    std::vector<std::string> read_response(std::size_t request_id) {
        std::vector<std::string> lines;
        const std::string expected = "result " + std::to_string(request_id);
        const std::string first = read_line(config_.response_timeout);
        if (first != expected) {
            throw std::runtime_error(
                "external AI response does not begin with expected result frame"
            );
        }
        lines.push_back(first);

        while (true) {
            const std::string line = read_line(config_.response_timeout);
            if (line == "end") break;
            lines.push_back(line);
        }
        return lines;
    }

    void start() {
#ifdef _WIN32
        SECURITY_ATTRIBUTES security{};
        security.nLength = sizeof(security);
        security.bInheritHandle = TRUE;

        HANDLE child_stdin_read = nullptr;
        HANDLE child_stdout_write = nullptr;
        if (!CreatePipe(&child_stdin_read, &stdin_write_, &security, 0)) {
            throw std::runtime_error("failed to create process AI stdin pipe");
        }
        if (!SetHandleInformation(stdin_write_, HANDLE_FLAG_INHERIT, 0)) {
            CloseHandle(child_stdin_read);
            CloseHandle(stdin_write_);
            stdin_write_ = nullptr;
            throw std::runtime_error("failed to configure process AI stdin pipe");
        }
        if (!CreatePipe(&stdout_read_, &child_stdout_write, &security, 0)) {
            CloseHandle(child_stdin_read);
            CloseHandle(stdin_write_);
            stdin_write_ = nullptr;
            throw std::runtime_error("failed to create process AI stdout pipe");
        }
        if (!SetHandleInformation(stdout_read_, HANDLE_FLAG_INHERIT, 0)) {
            CloseHandle(child_stdin_read);
            CloseHandle(stdin_write_);
            CloseHandle(stdout_read_);
            CloseHandle(child_stdout_write);
            stdin_write_ = nullptr;
            stdout_read_ = nullptr;
            throw std::runtime_error("failed to configure process AI stdout pipe");
        }

        std::string command = quote_windows_arg(config_.executable);
        for (const std::string& argument : config_.arguments) {
            command += ' ';
            command += quote_windows_arg(argument);
        }
        std::vector<char> command_buffer(command.begin(), command.end());
        command_buffer.push_back('\0');

        STARTUPINFOA startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = child_stdin_read;
        startup.hStdOutput = child_stdout_write;
        startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        PROCESS_INFORMATION process_info{};
        const BOOL created = CreateProcessA(
            nullptr,
            command_buffer.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startup,
            &process_info
        );

        CloseHandle(child_stdin_read);
        CloseHandle(child_stdout_write);
        if (!created) {
            CloseHandle(stdin_write_);
            CloseHandle(stdout_read_);
            stdin_write_ = nullptr;
            stdout_read_ = nullptr;
            throw std::runtime_error("failed to start process AI");
        }

        process_ = process_info.hProcess;
        CloseHandle(process_info.hThread);
#else
        int child_stdin[2]{};
        int child_stdout[2]{};
        if (::pipe(child_stdin) != 0) {
            throw std::runtime_error("failed to create process AI stdin pipe");
        }
        if (::pipe(child_stdout) != 0) {
            ::close(child_stdin[0]);
            ::close(child_stdin[1]);
            throw std::runtime_error("failed to create process AI stdout pipe");
        }

        const pid_t child = ::fork();
        if (child < 0) {
            ::close(child_stdin[0]);
            ::close(child_stdin[1]);
            ::close(child_stdout[0]);
            ::close(child_stdout[1]);
            throw std::runtime_error("failed to fork process AI");
        }

        if (child == 0) {
            ::dup2(child_stdin[0], STDIN_FILENO);
            ::dup2(child_stdout[1], STDOUT_FILENO);
            ::close(child_stdin[0]);
            ::close(child_stdin[1]);
            ::close(child_stdout[0]);
            ::close(child_stdout[1]);

            std::vector<std::string> storage;
            storage.reserve(config_.arguments.size() + 1);
            storage.push_back(config_.executable);
            storage.insert(storage.end(), config_.arguments.begin(), config_.arguments.end());

            std::vector<char*> argv;
            argv.reserve(storage.size() + 1);
            for (std::string& item : storage) argv.push_back(item.data());
            argv.push_back(nullptr);
            ::execvp(config_.executable.c_str(), argv.data());
            ::_exit(127);
        }

        pid_ = child;
        stdin_fd_ = child_stdin[1];
        stdout_fd_ = child_stdout[0];
        ::close(child_stdin[0]);
        ::close(child_stdout[1]);
#endif
    }

    void stop() noexcept {
#ifdef _WIN32
        if (stdin_write_ != nullptr) {
            CloseHandle(stdin_write_);
            stdin_write_ = nullptr;
        }
        if (process_ != nullptr) {
            if (WaitForSingleObject(process_, 100) == WAIT_TIMEOUT) {
                TerminateProcess(process_, 1);
                WaitForSingleObject(process_, 1000);
            }
            CloseHandle(process_);
            process_ = nullptr;
        }
        if (stdout_read_ != nullptr) {
            CloseHandle(stdout_read_);
            stdout_read_ = nullptr;
        }
#else
        if (stdin_fd_ >= 0) {
            ::close(stdin_fd_);
            stdin_fd_ = -1;
        }
        if (pid_ > 0) {
            int status = 0;
            for (int attempt = 0; attempt < 10; ++attempt) {
                const pid_t result = ::waitpid(pid_, &status, WNOHANG);
                if (result == pid_ || (result < 0 && errno == ECHILD)) {
                    pid_ = -1;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (pid_ > 0) {
                ::kill(pid_, SIGTERM);
                (void)::waitpid(pid_, &status, 0);
                pid_ = -1;
            }
        }
        if (stdout_fd_ >= 0) {
            ::close(stdout_fd_);
            stdout_fd_ = -1;
        }
#endif
    }

    void write_all(const std::string& data) {
#ifdef _WIN32
        std::size_t offset = 0;
        while (offset < data.size()) {
            DWORD written = 0;
            const DWORD chunk = static_cast<DWORD>(
                std::min<std::size_t>(data.size() - offset, 1U << 20)
            );
            if (!WriteFile(
                    stdin_write_,
                    data.data() + offset,
                    chunk,
                    &written,
                    nullptr)
                || written == 0) {
                throw std::runtime_error("failed to write process AI request");
            }
            offset += written;
        }
#else
        std::size_t offset = 0;
        while (offset < data.size()) {
            const ssize_t written = write_without_sigpipe(
                stdin_fd_,
                data.data() + offset,
                data.size() - offset
            );
            if (written < 0) {
                if (errno == EINTR) continue;
                if (errno == EPIPE) {
                    throw std::runtime_error("process AI closed stdin");
                }
                throw std::runtime_error("failed to write process AI request");
            }
            if (written == 0) {
                throw std::runtime_error("process AI stdin closed");
            }
            offset += static_cast<std::size_t>(written);
        }
#endif
    }

    std::string read_line(std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (true) {
            const std::size_t newline = read_buffer_.find('\n');
            if (newline != std::string::npos) {
                std::string line = read_buffer_.substr(0, newline);
                read_buffer_.erase(0, newline + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                return line;
            }

            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                throw std::runtime_error("process AI response timed out");
            }

#ifdef _WIN32
            DWORD available = 0;
            if (!PeekNamedPipe(stdout_read_, nullptr, 0, nullptr, &available, nullptr)) {
                throw std::runtime_error("process AI stdout pipe closed");
            }
            if (available == 0) {
                if (WaitForSingleObject(process_, 0) == WAIT_OBJECT_0) {
                    throw std::runtime_error("process AI exited before response completed");
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            char buffer[4096];
            const DWORD wanted = std::min<DWORD>(
                available,
                static_cast<DWORD>(sizeof(buffer))
            );
            DWORD read = 0;
            if (!ReadFile(stdout_read_, buffer, wanted, &read, nullptr) || read == 0) {
                throw std::runtime_error("failed to read process AI response");
            }
            read_buffer_.append(buffer, buffer + read);
#else
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - now
            );
            pollfd descriptor{};
            descriptor.fd = stdout_fd_;
            descriptor.events = POLLIN | POLLHUP;
            const int wait_ms = static_cast<int>(
                std::max<std::int64_t>(1, remaining.count())
            );
            const int poll_result = ::poll(&descriptor, 1, wait_ms);
            if (poll_result < 0) {
                if (errno == EINTR) continue;
                throw std::runtime_error("failed while waiting for process AI response");
            }
            if (poll_result == 0) continue;

            char buffer[4096];
            const ssize_t read = ::read(stdout_fd_, buffer, sizeof(buffer));
            if (read < 0) {
                if (errno == EINTR) continue;
                throw std::runtime_error("failed to read process AI response");
            }
            if (read == 0) {
                throw std::runtime_error("process AI exited before response completed");
            }
            read_buffer_.append(buffer, static_cast<std::size_t>(read));
#endif
        }
    }

    PersistentProcessBackendConfig config_;
    std::string read_buffer_;
#ifdef _WIN32
    HANDLE process_{nullptr};
    HANDLE stdin_write_{nullptr};
    HANDLE stdout_read_{nullptr};
#else
    pid_t pid_{-1};
    int stdin_fd_{-1};
    int stdout_fd_{-1};
#endif
};

PersistentProcessAIBackend::PersistentProcessAIBackend(
    PersistentProcessBackendConfig config)
    : config_(std::move(config)),
      impl_(std::make_unique<Impl>(config_)) {}

PersistentProcessAIBackend::~PersistentProcessAIBackend() = default;
PersistentProcessAIBackend::PersistentProcessAIBackend(
    PersistentProcessAIBackend&&) noexcept = default;
PersistentProcessAIBackend& PersistentProcessAIBackend::operator=(
    PersistentProcessAIBackend&&) noexcept = default;

AIBackendKind PersistentProcessAIBackend::kind() const noexcept {
    return config_.kind;
}

std::string PersistentProcessAIBackend::name() const {
    return config_.name;
}

SearchResult PersistentProcessAIBackend::decide(
    const Position& position,
    const SearchLimits& limits) {
    return impl_->decide(next_request_id_++, position, limits);
}

} // namespace kadoka::shogi::runtime
