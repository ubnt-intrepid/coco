#include <thread>
#include <iostream>
#include <string>

#include "arc.hh"
#include "channel.hh"

int main()
{
  sender<bool> tx;
  receiver<bool> rx;
  std::tie(tx, rx) = make_channel<bool>();

  arc<std::string> message;

  std::thread th([ rx = std::move(rx), message ]() mutable {
    // block until a message is received.
    while (rx.recv()) {
      auto received = message.lock().get();
      std::cout << "received: " << received << std::endl;
    }
  });

  for (int i = 0; i < 10; ++i) {
    *message.lock() = std::to_string(i);
    tx.send(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
  tx.send(false);

  th.join();
}
