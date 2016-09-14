#include "coco.hh"
#include <locale>
#include <iostream>

int main(int argc, char const* argv[])
{
  std::setlocale(LC_ALL, "");

  try {
    // Initialize Coco application.
    Config config{argc, argv};
    Coco coco{config};

    // retrieve a selection from lines.
    auto selected_lines = coco.select_line();

    // show selected line.
    for (auto&& line : selected_lines)
      std::cout << line << std::endl;

    return 0;
  }
  catch (std::exception& e) {
    std::cerr << "An error is thrown: " << e.what() << std::endl;
    return -1;
  }
}
