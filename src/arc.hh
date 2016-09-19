#ifndef __HEADER_ARC__
#define __HEADER_ARC__

#include <functional>
#include <memory>
#include <mutex>

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

template <typename T, typename Mutex = std::mutex>
class lockable {
  T body;
  Mutex mutex;

public:
  lockable() = default;
  lockable(T body) : body{std::move(body)} {}

  locked<T, Mutex> lock() { return {body, mutex}; }

  template <typename Func>
  locked<T, Mutex> lock(Func func)
  {
    auto&& l = locked<T, Mutex>{body, mutex};
    func(l.get());
    return std::move(l);
  }

  T const& raw() const noexcept { return body; }
};

template <typename T>
class arc {
  std::shared_ptr<lockable<T>> body;

public:
  arc() : body{std::make_shared<lockable<T>>()} {}
  arc(T body) : body{std::make_shared<lockable<T>>(std::move(body))} {}

  locked<T, std::mutex> lock() { return body->lock(); }

  template <typename Func>
  locked<T, std::mutex> lock(Func func)
  {
    return body->lock(std::move(func));
  }

  lockable<T>& get() { return *body; }
};

#endif
