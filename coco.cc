#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <locale>
#include <numeric>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>
#include <tuple>

#include <nanojson.hpp>
#include <cmdline.h>
#include "ncurses.hh"
#include "utf8.hh"

constexpr size_t y_offset = 1;

struct Config {
  std::vector<std::string> lines;
  std::string prompt;
  std::string query;
  std::size_t score_max;
  std::size_t max_buffer;

public:
  Config() = default;
  Config(int argc, char const** argv) { read_from(argc, argv); }

  void read_from(int argc, char const** argv)
  {
    cmdline::parser parser;
    parser.set_program_name("coco");
    parser.add("help", 'h', "show this message and quit");
    parser.add<std::string>("query", 0, "initial value for query", false, "");
    parser.add<std::string>("prompt", 0, "specify the prompt string", false, "QUERY> ");
    parser.add<std::size_t>("max-buffer", 'b', "maximum length of lines", false, 4096);
    parser.add<std::size_t>("score-max", 's', "maximum of scorering number", false,
                            std::numeric_limits<std::size_t>::max() / 2);
    parser.footer("filename...");
    parser.parse_check(argc, argv);

    query = parser.get<std::string>("query");
    prompt = parser.get<std::string>("prompt");
    score_max = parser.get<std::size_t>("score-max");
    max_buffer = parser.get<std::size_t>("max-buffer");

    lines.resize(0);
    lines.reserve(max_buffer);
    if (parser.rest().size() > 0) {
      for (auto&& path : parser.rest()) {
        std::ifstream ifs{path};
        read_lines(ifs, max_buffer);
      }
    }
    else {
      read_lines(std::cin, max_buffer);
    }
  }

  void read_lines(std::istream& is, std::size_t max_len)
  {
    static std::regex ansi(R"(\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[m|K])");
    for (std::string line; std::getline(is, line);) {
      if (lines.size() == max_len)
        return;
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

  std::vector<std::size_t> filtered;
  std::string query;
  size_t cursor = 0;
  size_t offset = 0;

public:
  Coco(Config const& config) : config(config)
  {
    query = config.query;
    update_filter_list();
  }

  Selection select_line()
  {
    // initialize ncurses screen.
    Ncurses term;
    render_screen(term);

    while (true) {
      Event ev = term.poll_event();
      auto result = handle_key_event(term, ev);

      if (result == Status::Selected) {
        return Selection{true, config.lines[filtered[cursor + offset]]};
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
      term.add_str(0, y + 1, config.lines[filtered[y + offset]]);
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

      if (cursor == static_cast<size_t>(height - 1 - y_offset)) {
        offset = std::min<size_t>(offset + 1, std::max<int>(0, filtered.size() - height + y_offset));
      }
      else {
        cursor = std::min<size_t>(cursor + 1, std::min<size_t>(filtered.size() - offset, height - y_offset) - 1);
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

  auto regex_score() const
  {
    std::regex re(query);
    return [re = std::move(re)](std::string const& line)->std::size_t
    {
      return std::regex_search(line, re) ? 1 : std::numeric_limits<std::size_t>::max();
    };
  }

  void update_filter_list()
  {
    choice(filtered, config.lines, regex_score(), config.score_max);
    cursor = 0;
    offset = 0;
  }

  template <typename Scorer>
  void choice(std::vector<std::size_t>& filtered, std::vector<std::string> const& lines, Scorer score,
              std::size_t score_max) const
  {
    filtered.resize(lines.size());
    std::iota(filtered.begin(), filtered.end(), 0);

    if (!query.empty()) {
      auto pos = std::stable_partition(filtered.begin(), filtered.end(), [
        score = std::move(score), score_max, lines = static_cast<std::vector<std::string> const&>(lines)
      ](std::size_t idx) { return score(lines[idx]) <= score_max; });
      if (pos < filtered.end()) {
        filtered.resize(pos - filtered.begin());
      }
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
