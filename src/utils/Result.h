#pragma once

#include <QString>
#include <optional>
#include <utility>

struct AudioError
{
    enum class Code {
        None = 0,
        FileNotFound,
        UnsupportedFormat,
        DecodeError,
        EncodeError,
        StretchError,
        IoError,
        Cancelled,
        Unknown,
    };

    Code    code = Code::None;
    QString message;
    QString filePath;

    bool isError() const { return code != Code::None; }
    static AudioError ok() { return {}; }
    static AudioError make(Code c, QString msg, QString path = {})
    {
        return AudioError{ c, std::move(msg), std::move(path) };
    }
};

// Лёгкий аналог std::expected<T, E> для передачи через Qt-сигналы.
template <typename T>
class Result
{
public:
    Result(T value) : m_value(std::move(value)) {}
    Result(AudioError err) : m_error(std::move(err)) {}

    bool hasValue() const { return m_value.has_value(); }
    bool hasError() const { return !hasValue(); }

    const T& value() const { return *m_value; }
    T& value() { return *m_value; }

    const AudioError& error() const { return m_error; }

private:
    std::optional<T> m_value;
    AudioError       m_error;
};

template <>
class Result<void>
{
public:
    Result() = default;
    Result(AudioError err) : m_error(std::move(err)) {}

    bool hasError() const { return m_error.isError(); }
    const AudioError& error() const { return m_error; }

private:
    AudioError m_error;
};