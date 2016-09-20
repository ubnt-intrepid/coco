#ifndef __HEADER_ARC__
#define __HEADER_ARC__

#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>

template <class T, class Mutex>
class locked {
  std::reference_wrapper<T> body;
  std::unique_lock<Mutex> lock;

public:
  locked() = delete;
  locked(locked const&) = delete;
  locked& operator=(locked const&) = delete;
  locked(locked&&) noexcept = default;
  locked& operator=(locked&&) noexcept = default;

  locked(T& body, Mutex& mtx) : body{body}, lock{mtx} {}

  T& operator*() const noexcept { return get(); }

  T& get() const noexcept { return body; }
  operator bool() const noexcept { return true; }
};

template <class T, class Mutex>
class locked_shared {
  std::reference_wrapper<T> body;
  std::shared_lock<Mutex> lock;

public:
  locked_shared() = delete;
  locked_shared(locked_shared const&) = delete;
  locked_shared& operator=(locked_shared const&) = delete;
  locked_shared(locked_shared&&) noexcept = default;
  locked_shared& operator=(locked_shared&&) noexcept = default;

  locked_shared(T& body, Mutex& mtx) : body{body}, lock{mtx} {}

  T& operator*() const noexcept { return get(); }

  T& get() const noexcept { return body; }
  operator bool() const noexcept { return true; }
};

template <typename T, typename Mutex = std::mutex>
class lockable {
  T body;
  Mutex mutex;

public:
  lockable() = default;
  lockable(T body) : body{std::move(body)} {}

  locked<T, Mutex> lock() { return {body, mutex}; }

  locked_shared<T, Mutex> read() { return {body, mutex}; }

  T const& raw() const noexcept { return body; }
};

template <typename T, typename Mutex = std::shared_timed_mutex>
class arc {
  std::shared_ptr<lockable<T, Mutex>> body;

public:
  arc() : body{std::make_shared<lockable<T, Mutex>>()} {}
  arc(T body) : body{std::make_shared<lockable<T, Mutex>>(std::move(body))} {}

  locked<T, std::mutex> lock() { return body->lock(); }
  locked_shared<T, std::shared_timed_mutex> read() { return body->read(); }

  lockable<T>& get() { return *body; }
};

#endif
