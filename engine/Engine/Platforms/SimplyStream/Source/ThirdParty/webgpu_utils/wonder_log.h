#pragma once

#include <string_view>
#include <optional>
#include <vector>
#include <algorithm>
#include <source_location>
#include <atomic>
#include <span>
#include <concepts>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <array>
#include <mutex>
#include <type_traits>

// --- Configuration Macros ---
// #define WONDER_LOG_ENABLE_CONSOLE
// #define WONDER_LOG_ENABLE_FILE
// #define WONDER_LOG_ENABLE_THREAD_ID
// #define WONDER_LOG_ENABLE_VERBOSE
// #define WONDER_LOG_ENABLE_INFO
// #define WONDER_LOG_ENABLE_WARNING
// #define WONDER_LOG_ENABLE_ERROR
// #define WONDER_LOG_USE_RING_BUFFER
// #define WONDER_LOG_FILE_NAME "app_log.txt"

// --- Derived Configuration ---
#ifndef WONDER_LOG_FILE_NAME
#define WONDER_LOG_FILE_NAME "default_log.log"
#endif

#if defined(WONDER_LOG_DISABLE_FILE) && defined(WONDER_LOG_USE_RING_BUFFER)
#warning "WONDER_LOG_USE_RING_BUFFER has no effect when WONDER_LOG_DISABLE_FILE is defined."
#undef WONDER_LOG_USE_RING_BUFFER // Ensure ring buffer is off if file is off
#endif

#if defined(WONDER_LOG_DISABLE_ERROR)
#define WONDER_LOG_DISABLE_FATAL_LOGGING
#endif

// --- Formatting Library ---
#if __has_include(<print>)
#include <format>
namespace format_ns = std;
template <typename... Args>
using check_format_string = std::format_string<Args...>;
#else
#define FMT_HEADER_ONLY
#define FMT_NO_EXCEPTIONS
#include "fmt/format.h"
namespace format_ns = fmt;
template <typename... Args>
using check_format_string = fmt::format_string<Args...>;
#endif

// --- Platform Specifics ---
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#if defined(UE_BUILD) || defined(UE_EDITOR)
#include "Windows/WindowsHWrapper.h"
#else
#include <Windows.h>
#endif
#else
#if !defined(__EMSCRIPTEN__) && !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#endif
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define TRACE_LOCATION std::source_location::current()

enum class WonderLogLevel { Verbose, Info, Warning, Error, Fatal, Force };

#define WONDER_LOG_USE_RING_BUFFER 1

// Compile-time settings structure
struct WonderLogSettings {
    static constexpr bool log_to_console = true;
    static constexpr bool log_to_file = false;
    static constexpr bool log_with_thread_id = false;
    static constexpr bool log_with_source_location = true; // Controls if location is *printed*, not if it's captured
    static constexpr bool use_ring_buffer = WONDER_LOG_USE_RING_BUFFER;
    static constexpr bool types_to_log[6] = {
        true,
        true,
        true,
        true,
        true,
        true // Force
    };

    [[nodiscard]] static constexpr bool is_level_log_enabled(WonderLogLevel level) noexcept {
        if (level == WonderLogLevel::Force) return true;
        const size_t index = static_cast<size_t>(level);
        if (index < std::size(types_to_log)) {
            return types_to_log[index];
        }
        return false;
    }
};

template<WonderLogLevel Level>
concept IsLogLevelEnabled = WonderLogSettings::is_level_log_enabled(Level);

namespace wonder_log_detail {

enum class OutputType { Log, Error, Fatal };

// --- Ring Buffer ---
#if defined(WONDER_LOG_USE_RING_BUFFER)
struct RingBuffer {
    // ... (Ring Buffer implementation remains identical) ...
    static constexpr size_t SLOT_SIZE = 4096;
    static constexpr size_t SLOT_COUNT = 16;
    static constexpr size_t SLOT_MASK = SLOT_COUNT - 1;

    struct alignas(64) Slot {
        char data[SLOT_SIZE];
        std::atomic<size_t> size{0};
        std::atomic<uint64_t> sequence{0};
    };

    alignas(64) std::array<Slot, SLOT_COUNT> slots{};
    alignas(64) std::atomic<size_t> write_index{0};
    alignas(64) std::atomic<uint64_t> next_sequence{1};
    alignas(64) std::atomic<uint64_t> next_flush_sequence{1};

    bool push(std::string_view sv) noexcept {
        if (sv.size() >= SLOT_SIZE) { return false; }
        size_t index = write_index.fetch_add(1, std::memory_order_relaxed) & SLOT_MASK;
        uint64_t seq = next_sequence.fetch_add(1, std::memory_order_relaxed);
        while (slots[index].sequence.load(std::memory_order_acquire) != 0) {
             std::this_thread::yield(); // Or return false;
        }
        std::memcpy(slots[index].data, sv.data(), sv.size());
        slots[index].sequence.store(seq, std::memory_order_release);
        slots[index].size.store(sv.size(), std::memory_order_release);
        return true;
    }

    void collect_ordered(auto& write_func) noexcept {
         uint64_t current_expected_seq = next_flush_sequence.load(std::memory_order_acquire);
         for (size_t i = 0; i < SLOT_COUNT * 2; ++i) {
             const size_t current_slot_idx = (write_index.load(std::memory_order_relaxed) - 1 - i) & SLOT_MASK;
             uint64_t slot_seq = slots[current_slot_idx].sequence.load(std::memory_order_acquire);

             if (slot_seq == current_expected_seq) {
                  size_t slot_size = slots[current_slot_idx].size.load(std::memory_order_acquire);
                  if (slot_size > 0 && slot_size < SLOT_SIZE) {
                        write_func(std::string_view(slots[current_slot_idx].data, slot_size));
                        slots[current_slot_idx].sequence.store(0, std::memory_order_release);
                        slots[current_slot_idx].size.store(0, std::memory_order_relaxed);
                        current_expected_seq++;
                        next_flush_sequence.store(current_expected_seq, std::memory_order_release);
                        i = static_cast<size_t>(-1); // Restart search
                  } else { break; } // Error or race? Wait.
             } else if(slot_seq != 0 && slot_seq < current_expected_seq) {
                  // Clear stale entry
                  slots[current_slot_idx].sequence.store(0, std::memory_order_release);
                  slots[current_slot_idx].size.store(0, std::memory_order_relaxed);
             }
        }
    }
};
inline RingBuffer g_ring_buffer; // Global instance
#endif // defined(WONDER_LOG_USE_RING_BUFFER)

// --- File Writer ---
#if !defined(WONDER_LOG_DISABLE_FILE)
class FileWriter {
private:
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
    HANDLE file_handle_ = INVALID_HANDLE_VALUE;
#elif !defined(__EMSCRIPTEN__) && !defined(_WIN32)
    int fd_ = -1;
#endif
    bool file_open_ = false;
    std::atomic_flag flush_lock_ = ATOMIC_FLAG_INIT;

    void write_raw(std::string_view sv) noexcept {
        // write_raw implementation remains the same
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
        if (file_handle_ != INVALID_HANDLE_VALUE) {
            DWORD bytes_written;
            WriteFile(file_handle_, sv.data(), static_cast<DWORD>(sv.size()), &bytes_written, nullptr);
            WriteFile(file_handle_, "\n", 1, &bytes_written, nullptr);
        }
#elif !defined(__EMSCRIPTEN__) && !defined(_WIN32)
        if (fd_ >= 0) {
            ::write(fd_, sv.data(), sv.size());
            ::write(fd_, "\n", 1);
        }
#endif
    }

public:
    FileWriter() = default;
    ~FileWriter() {
        close_file();
    }

    FileWriter(const FileWriter&) = delete;
    FileWriter& operator=(const FileWriter&) = delete;
    FileWriter(FileWriter&&) = delete;
    FileWriter& operator=(FileWriter&&) = delete;

    void open_file() noexcept {
        // open_file implementation remains the same
        if (file_open_) return;
        while (flush_lock_.test_and_set(std::memory_order_acquire)) { std::this_thread::yield(); }
        if (!file_open_) {
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
            file_handle_ = CreateFileA(WONDER_LOG_FILE_NAME, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            file_open_ = (file_handle_ != INVALID_HANDLE_VALUE);
            if (!file_open_ && WonderLogSettings::log_to_console) { // Use constexpr here
                 fprintf(stderr, "[Logger Error] Failed to open log file '%s'. Code: %lu\n", WONDER_LOG_FILE_NAME, GetLastError());
            }
#elif !defined(__EMSCRIPTEN__) && !defined(_WIN32)
            fd_ = ::open(WONDER_LOG_FILE_NAME, O_WRONLY | O_APPEND | O_CREAT, 0644);
            file_open_ = (fd_ >= 0);
            if (!file_open_ && WonderLogSettings::log_to_console) { // Use constexpr here
                 fprintf(stderr, "[Logger Error] Failed to open log file '%s'. Errno: %d\n", WONDER_LOG_FILE_NAME, errno);
            }
#endif
        }
        flush_lock_.clear(std::memory_order_release);
    }

    void close_file() noexcept {
        // close_file implementation remains the same
        while (flush_lock_.test_and_set(std::memory_order_acquire)) { std::this_thread::yield(); }
        if (file_open_) {
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
            if (file_handle_ != INVALID_HANDLE_VALUE) { CloseHandle(file_handle_); file_handle_ = INVALID_HANDLE_VALUE;}
#elif !defined(__EMSCRIPTEN__) && !defined(_WIN32)
            if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
#endif
            file_open_ = false;
        }
        flush_lock_.clear(std::memory_order_release);
    }

// Use the original macro for conditional compilation block
#if defined(WONDER_LOG_USE_RING_BUFFER)
    void flush_buffer() noexcept {
        // flush_buffer implementation remains the same
        if (!file_open_) { open_file(); if (!file_open_) return; }
        while (flush_lock_.test_and_set(std::memory_order_acquire)) { std::this_thread::yield(); }
        if (file_open_) {
             auto writer_lambda = [this](std::string_view sv) { this->write_raw(sv); };
             g_ring_buffer.collect_ordered(writer_lambda); // g_ring_buffer defined only if macro is set
        }
        flush_lock_.clear(std::memory_order_release);
    }
#endif // defined(WONDER_LOG_USE_RING_BUFFER)

// Use the original macro for conditional compilation block
#if !defined(WONDER_LOG_USE_RING_BUFFER)
    // Direct write (used when ring buffer disabled but file logging enabled)
    void write_direct(std::string_view sv) noexcept {
        if (!file_open_) { open_file(); if (!file_open_) return; }
        write_raw(sv);
    }
#endif // !defined(WONDER_LOG_USE_RING_BUFFER)
};

// Global file writer instance
inline FileWriter g_file_writer;
inline std::once_flag g_file_init_flag;

inline void ensure_file_opened() {
    std::call_once(g_file_init_flag, [](){ g_file_writer.open_file(); });
}

#endif // !defined(WONDER_LOG_DISABLE_FILE)


// --- Platform Output ---
inline void platform_output(OutputType type, std::string_view sv) noexcept {
    if (sv.empty()) [[unlikely]] return;

    // 1. Console Output
    if constexpr (WonderLogSettings::log_to_console) {
#ifdef __EMSCRIPTEN__
        EM_ASM({ /* ... Emscripten console output ... */
            var str = UTF8ToString($0, $1);
            if ($2) console.error(str); else console.log(str);
        }, sv.data(), (uint32_t)sv.size(), type == OutputType::Error || type == OutputType::Fatal);
#else
        FILE* target_stream = (type == OutputType::Error || type == OutputType::Fatal) ? stderr : stdout;
        fwrite(sv.data(), 1, sv.size(), target_stream);
        fputc('\n', target_stream);
        if (type != OutputType::Log) {
             fflush(target_stream);
        }
#endif
    }

    // 2. File Output
    if constexpr (WonderLogSettings::log_to_file) {
        ensure_file_opened(); // This function only defined if !WONDER_LOG_DISABLE_FILE
#if defined(WONDER_LOG_USE_RING_BUFFER) // Use macro for conditional compilation
        if (!g_ring_buffer.push(sv)) [[unlikely]] { // g_ring_buffer only defined if macro set
             if constexpr (WonderLogSettings::log_to_console) {
                 fprintf(stderr, "[Logger Warn] Dropped file log (size %zu): Ring buffer push failed.\n", sv.size());
             }
        }
#else // Direct write (implies !WONDER_LOG_DISABLE_FILE)
        g_file_writer.write_direct(sv); // g_file_writer only defined if !WONDER_LOG_DISABLE_FILE
#endif
    }

    // 3. Handle Fatal Exit
    if (type == OutputType::Fatal) [[unlikely]] {
// Use macro for conditional compilation block
#if defined(WONDER_LOG_USE_RING_BUFFER)
        g_file_writer.flush_buffer(); // g_file_writer and flush_buffer only defined if needed
#endif
        exit(EXIT_FAILURE);
    }
}

// --- Formatting and Dispatch ---
template <WonderLogLevel level, typename... Args>
inline void format_and_dispatch(
    const std::source_location* loc_ptr, // Moved loc_ptr to the front
    check_format_string<Args...> format,
    Args&&... args
) noexcept
{
    constexpr OutputType output_type = (level == WonderLogLevel::Error) ? OutputType::Error :
                                      (level == WonderLogLevel::Fatal) ? OutputType::Fatal :
                                      OutputType::Log;

    if constexpr (!WonderLogSettings::is_level_log_enabled(level)) {
         if constexpr (level == WonderLogLevel::Fatal) {
             exit(EXIT_FAILURE);
         }
         return;
    }

     if constexpr (WonderLogSettings::log_to_console || WonderLogSettings::log_to_file)
     {
        thread_local std::array<char, 1024> local_buffer;
        thread_local std::string fallback_buffer;

        std::string prefix_str;
        if constexpr (WonderLogSettings::log_with_thread_id) {
             auto tid = std::this_thread::get_id();
             prefix_str += format_ns::format("[{}]", std::hash<std::thread::id>{}(tid));
        }
        if constexpr (WonderLogSettings::log_with_source_location) {
            if (loc_ptr) {
                 prefix_str += format_ns::format("[{}:{}:{}]: ",
                                                 loc_ptr->file_name(),
                                                 loc_ptr->function_name(),
                                                 loc_ptr->line());
            }
        } else if constexpr (WonderLogSettings::log_with_thread_id) {
             if (!prefix_str.empty()) prefix_str += ": ";
        }
        std::string_view prefix_view = prefix_str;

        char* format_buffer_start = local_buffer.data();
        std::size_t format_buffer_capacity = local_buffer.size();
        bool use_fallback = false;

        if (!prefix_view.empty()) {
            if (prefix_view.size() < format_buffer_capacity) {
                 std::memcpy(format_buffer_start, prefix_view.data(), prefix_view.size());
                 format_buffer_start += prefix_view.size();
                 format_buffer_capacity -= prefix_view.size();
            } else {
                 use_fallback = true;
            }
        }

        std::string_view final_output_view;

        if (!use_fallback) {
            try {
                auto result = format_ns::format_to_n(format_buffer_start, format_buffer_capacity -1, format, std::forward<Args>(args)...);

                if (static_cast<std::size_t>(result.size) < format_buffer_capacity) {
                    final_output_view = std::string_view(local_buffer.data(), (format_buffer_start - local_buffer.data()) + result.size);
                } else {
                     use_fallback = true;
                }
            } catch (const format_ns::format_error& e) {
                 if constexpr (WonderLogSettings::log_to_console) { fprintf(stderr, "[Logger Fmt Err] %s\n", e.what()); }
                 return;
            } catch (...) {
                 if constexpr (WonderLogSettings::log_to_console) { fprintf(stderr, "[Logger Fmt Err] Unknown\n"); }
                 return;
            }
        }

        if (use_fallback) {
             fallback_buffer.clear();
             fallback_buffer.append(prefix_view);
             try {
                 format_ns::format_to(std::back_inserter(fallback_buffer), format, std::forward<Args>(args)...);
                 final_output_view = fallback_buffer;
             } catch (const format_ns::format_error& e) {
                 if constexpr (WonderLogSettings::log_to_console) { fprintf(stderr, "[Logger Fmt Err FB] %s\n", e.what()); }
                 return;
             } catch (...) {
                 if constexpr (WonderLogSettings::log_to_console) { fprintf(stderr, "[Logger Fmt Err FB] Unknown\n"); }
                 return;
             }
        }

        platform_output(output_type, final_output_view);

     } else {
         if constexpr (level == WonderLogLevel::Fatal) {
             exit(EXIT_FAILURE);
         }
     }
}

} // namespace wonder_log_detail

// --- Public API ---
class wonder_log {
public:
    static void flush() noexcept {
#if defined(WONDER_LOG_USE_RING_BUFFER)
        wonder_log_detail::g_file_writer.flush_buffer();
#endif
    }

    static void initialize_file_logging() noexcept {
// Use macro for conditional compilation block
#if !defined(WONDER_LOG_DISABLE_FILE)
        wonder_log_detail::ensure_file_opened();
#endif
    }

    // --- Verbose ---
    template <typename... Args>
    static void verbose(check_format_string<Args...> fmt, Args&&... args) {
        wonder_log_detail::format_and_dispatch<WonderLogLevel::Verbose>(nullptr, fmt, std::forward<Args>(args)...);
    }
    template <typename... Args>
    static void verbose(const std::source_location& loc, check_format_string<Args...> fmt, Args&&... args) {
        wonder_log_detail::format_and_dispatch<WonderLogLevel::Verbose>(&loc, fmt, std::forward<Args>(args)...);
    }

    // --- Info ---
    template <typename... Args>
    static void info(check_format_string<Args...> fmt, Args&&... args) {
        wonder_log_detail::format_and_dispatch<WonderLogLevel::Info>(nullptr, fmt, std::forward<Args>(args)...);
    }
     template <typename... Args>
    static void info(const std::source_location& loc, check_format_string<Args...> fmt, Args&&... args) {
        wonder_log_detail::format_and_dispatch<WonderLogLevel::Info>(&loc, fmt, std::forward<Args>(args)...);
    }

    // --- Warning ---
    template <typename... Args>
    static void warning(check_format_string<Args...> fmt, Args&&... args) {
        wonder_log_detail::format_and_dispatch<WonderLogLevel::Warning>(nullptr, fmt, std::forward<Args>(args)...);
    }
    template <typename... Args>
    static void warning(const std::source_location& loc, check_format_string<Args...> fmt, Args&&... args) {
        wonder_log_detail::format_and_dispatch<WonderLogLevel::Warning>(&loc, fmt, std::forward<Args>(args)...);
    }

    // --- Error ---
    template <typename... Args>
    static void error(check_format_string<Args...> fmt, Args&&... args) {
        wonder_log_detail::format_and_dispatch<WonderLogLevel::Error>(nullptr, fmt, std::forward<Args>(args)...);
    }
     template <typename... Args>
    static void error(const std::source_location& loc, check_format_string<Args...> fmt, Args&&... args) {
        wonder_log_detail::format_and_dispatch<WonderLogLevel::Error>(&loc, fmt, std::forward<Args>(args)...);
    }

    // --- Fatal ---
    template <typename... Args>
    [[ noreturn ]] static void fatal(check_format_string<Args...> fmt, Args&&... args) {
         wonder_log_detail::format_and_dispatch<WonderLogLevel::Fatal>(nullptr, fmt, std::forward<Args>(args)...);
         exit(EXIT_FAILURE);
    }
    template <typename... Args>
    [[ noreturn ]] static void fatal(const std::source_location& loc, check_format_string<Args...> fmt, Args&&... args) {
         wonder_log_detail::format_and_dispatch<WonderLogLevel::Fatal>(&loc, fmt, std::forward<Args>(args)...);
         exit(EXIT_FAILURE);
    }

    // --- Force ---
    template <typename... Args>
    static void force(check_format_string<Args...> format, Args&&... args) {
         wonder_log_detail::format_and_dispatch<WonderLogLevel::Force>(nullptr, format, std::forward<Args>(args)...);
     }
    template <typename... Args>
    static void force(const std::source_location& loc, check_format_string<Args...> format, Args&&... args) {
         wonder_log_detail::format_and_dispatch<WonderLogLevel::Force>(&loc, format, std::forward<Args>(args)...);
     }
};

// --- Cleanup Helper ---
#if defined(WONDER_LOG_USE_RING_BUFFER)
struct LoggerCleanup {
    ~LoggerCleanup() {
        wonder_log_detail::g_file_writer.flush_buffer();
    }
};
inline LoggerCleanup g_logger_cleanup_handler;
#endif // defined(WONDER_LOG_USE_RING_BUFFER)
