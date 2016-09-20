#include "coco.hh"
#include <locale>
#include <iostream>
#include <regex>
#include <fstream>

void read_lines(std::vector<std::string>& lines, std::istream& is, std::size_t max_len)
{
  static std::regex ansi(R"(\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[m|K])");
  for (std::string line; std::getline(is, line);) {
    if (lines.size() == max_len)
      return;
    lines.push_back(std::regex_replace(line, ansi, ""));
  }
}

Candidates get_candidates(std::string const& file, std::size_t max_buffer)
{
  std::vector<std::string> lines;
  lines.reserve(max_buffer);

  if (!file.empty()) {
    std::ifstream ifs{file};
    read_lines(lines, ifs, max_buffer);
  }
  else {
    read_lines(lines, std::cin, max_buffer);
  }
  return Candidates{{std::move(lines)}, max_buffer};
}

int main(int argc, char const* argv[])
{
  std::setlocale(LC_ALL, "");

  try {
    // Initialize Coco application.
    Config config;
    config.parse_args(argc, argv);

    Candidates candidates = get_candidates(config.file, config.max_buffer);

    Coco coco{config, candidates};

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
