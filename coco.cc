#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <locale>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>
#include <tuple>

#include <nanojson.hpp>
#include <cmdline.h>
#include "ncurses.hh"
#include "utf8.hh"

struct Config {
  std::vector<std::string> lines;
  std::string prompt = "QUERY> ";
  size_t y_offset = 1;

public:
  Config() = default;
  Config(int argc, char const** argv) { read_from(argc, argv); }

  void read_from(int argc, char const** argv)
  {
    static_cast<void>(argc);
    static_cast<void>(argv);

    std::ifstream ifs;
    read_lines((argc > 1) ? (ifs.open(argv[1]), ifs) : std::cin);
  }

  void read_lines(std::istream& is)
  {
    lines.resize(0);
    std::regex ansi(R"(\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[m|K])");
    for (std::string line; std::getline(is, line);) {
      lines.push_back(std::regex_replace(line, ansi, ""));
    }
  }
};

struct Selection {
  bool is_selected = false;
  std::string line;

public:
  inline operator bool() const { return is_selected; }
};

// represents a instance of Coco client.
class Coco {
  enum class Status {
    Selected,
    Escaped,
    Continue,
  };

  Config config;

  std::vector<std::string> filtered;
  std::string query;
  size_t cursor = 0;
  size_t offset = 0;

public:
  Coco(Config const& config) : config(config) { filtered = config.lines; }

  Selection select_line()
  {
    // initialize ncurses screen.
    Ncurses term;
    render_screen(term);

    while (true) {
      Event ev = term.poll_event();
      auto result = handle_key_event(term, ev);

      if (result == Status::Selected) {
        return Selection{true, filtered[cursor]};
      }
      else if (result == Status::Escaped) {
        break;
      }

      render_screen(term);
    }

    return Selection{};
  }

private:
  void render_screen(Ncurses& term)
  {
    std::string query_str = config.prompt + query;

    term.erase();

    int height;
    std::tie(std::ignore, height) = term.get_size();

    for (size_t y = 0; y < std::min<size_t>(filtered.size() - offset, height - 1); ++y) {
      term.add_str(0, y + 1, filtered[y + offset]);
      if (y == cursor) {
        term.change_attr(0, y + 1, -1, 2);
      }
    }

    term.add_str(0, 0, query_str);

    term.refresh();
  }

  Status handle_key_event(Ncurses& term, Event const& ev)
  {
    if (ev == Key::Enter) {
      return filtered.size() > 0 ? Status::Selected : Status::Escaped;
    }
    else if (ev == Key::Esc) {
      return Status::Escaped;
    }
    else if (ev == Key::Up) {
      if (cursor == 0) {
        offset = std::max(0, (int)offset - 1);
      }
      else {
        cursor--;
      }
      return Status::Continue;
    }
    else if (ev == Key::Down) {
      int height;
      std::tie(std::ignore, height) = term.get_size();

      if (cursor == static_cast<size_t>(height - 1 - config.y_offset)) {
        offset = std::min<size_t>(offset + 1, std::max<int>(0, filtered.size() - height + config.y_offset));
      }
      else {
        cursor = std::min<size_t>(cursor + 1, std::min<size_t>(filtered.size() - offset, height - config.y_offset) - 1);
      }
      return Status::Continue;
    }
    else if (ev == Key::Backspace) {
      if (!query.empty()) {
        pop_back_utf8(query);
        update_filter_list();
      }
      return Status::Continue;
    }
    else if (ev == Key::Char) {
      query += ev.as_chars();
      update_filter_list();
      return Status::Continue;
    }
    else {
      return Status::Continue;
    }
  }

  void update_filter_list()
  {
    filtered = filter_by_regex(config.lines);
    cursor = 0;
    offset = 0;
  }

  std::vector<std::string> filter_by_regex(std::vector<std::string> const& lines) const
  {
    if (query.empty()) {
      return lines;
    }
    else {
      std::regex re(query);
      std::vector<std::string> filtered;
      for (auto&& line : lines) {
        if (std::regex_search(line, re)) {
          filtered.push_back(line);
        }
      }
      return filtered;
    }
  }
};

int main(int argc, char const* argv[])
{
  std::setlocale(LC_ALL, "");

  try {
    // Initialize Coco application.
    Config config{argc, argv};
    Coco coco{config};

    // retrieve a selection from lines.
    auto selection = coco.select_line();

    // show selected line.
    if (selection) {
      std::cout << selection.line << std::endl;
    }

    return 0;
  }
  catch (std::exception& e) {
    std::cerr << "An error is thrown: " << e.what() << std::endl;
    return -1;
  }
}
