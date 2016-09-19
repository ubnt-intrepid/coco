#ifndef __HEADER_CHANNEL__
#define __HEADER_CHANNEL__

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <tuple>

template <typename T>
class channel {
  std::queue<T> queue;
  std::mutex m;
  std::condition_variable cv;

public:
  void send(T const& val)
  {
    std::unique_lock<std::mutex> lock{m};
    queue.push(val);
    cv.notify_one();
  }

  void send(T&& val)
  {
    std::unique_lock<std::mutex> lock{m};
    queue.push(val);
    cv.notify_one();
  }

  T recv()
  {
    std::unique_lock<std::mutex> lock{m};
    cv.wait(lock, [this] { return !queue.empty(); });
    T result = queue.front();
    queue.pop();
    return result;
  }
};

template <typename T>
class sender {
  std::shared_ptr<channel<T>> ch;

public:
  sender() = default;
  sender(std::shared_ptr<channel<T>> ch) : ch{std::move(ch)} {}

  void send()
  {
    if (ch)
      ch->send({});
  }
  void send(T const& val)
  {
    if (ch)
      ch->send(val);
  }
  void send(T&& val)
  {
    if (ch)
      ch->send(val);
  }
};

template <typename T>
class receiver {
  std::shared_ptr<channel<T>> ch;

public:
  receiver() = default;
  receiver(receiver const&) = delete;
  receiver(receiver&&) noexcept = default;
  receiver& operator=(receiver const&) noexcept = delete;
  receiver& operator=(receiver&&) noexcept = default;

  receiver(std::shared_ptr<channel<T>> ch) : ch{std::move(ch)} {}

  T recv()
  {
    if (!ch)
      throw std::logic_error("");
    return ch->recv();
  }
};

template <typename T>
auto make_channel()
{
  auto ch = std::make_shared<channel<T>>();
  return std::make_tuple(sender<T>{ch}, receiver<T>{ch});
}

#endif
