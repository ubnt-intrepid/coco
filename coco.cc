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
  bool multi_select;

public:
  Config() = default;
  Config(int argc, char const** argv) { read_from(argc, argv); }

  void read_from(int argc, char const** argv)
  {
    cmdline::parser parser;
    parser.set_program_name("coco");
    parser.add<std::string>("query", 0, "initial value for query", false, "");
    parser.add<std::string>("prompt", 0, "specify the prompt string", false, "QUERY> ");
    parser.add<std::size_t>("max-buffer", 'b', "maximum length of lines", false, 4096);
    parser.add<std::size_t>("score-max", 's', "maximum of scorering number", false,
                            std::numeric_limits<std::size_t>::max() / 2);
    parser.add("multi-select", 'm', "enable multiple selection of lines");
    parser.footer("filename...");
    parser.parse_check(argc, argv);

    query = parser.get<std::string>("query");
    prompt = parser.get<std::string>("prompt");
    score_max = parser.get<std::size_t>("score-max");
    max_buffer = parser.get<std::size_t>("max-buffer");
    multi_select = parser.exist("multi-select");

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

// represents a instance of Coco client.
class Coco {
  enum class Status {
    Selected,
    Escaped,
    Continue,
  };

  struct Choice {
    std::size_t index;
    std::size_t score = 0;
    bool selected = false;

  public:
    Choice() = default;
    Choice(std::size_t index) : index(index) {}

    bool operator<(Choice const& rhs) const { return score < rhs.score; }
  };

  Config config;
  std::string query;

  std::vector<Choice> choices;
  std::size_t filtered_len = 0;

  std::size_t cursor = 0;
  std::size_t offset = 0;

public:
  Coco(Config const& config) : config(config)
  {
    query = config.query;

    choices.resize(config.lines.size());
    std::generate(choices.begin(), choices.end(), [n = 0]() mutable { return Choice(n++); });
    update_filter_list();
  }

  std::vector<std::string> select_line()
  {
    // initialize ncurses screen.
    Ncurses term;
    render_screen(term);

    while (true) {
      Event ev = term.poll_event();
      auto result = handle_key_event(term, ev);

      if (result == Status::Selected) {
        std::vector<std::string> lines;
        for (std::size_t i = 0; i < filtered_len; ++i) {
          if (choices[i].selected)
            lines.push_back(config.lines[choices[i].index]);
        }
        return lines;
      }
      else if (result == Status::Escaped) {
        break;
      }

      render_screen(term);
    }

    return {};
  }

private:
  void render_screen(Ncurses& term)
  {
    std::string query_str = config.prompt + query;

    term.erase();

    int height;
    std::tie(std::ignore, height) = term.get_size();

    for (size_t y = 0; y < std::min<size_t>(filtered_len - offset, height - 1); ++y) {
      if (choices[y + offset].selected) {
        term.add_str(0, y + y_offset, ">");
      }
      term.add_str(2, y + 1, config.lines[choices[y + offset].index]);

      if (y == cursor) {
        term.change_attr(0, y + 1, -1, 0, A_BOLD | A_UNDERLINE);
      }
    }

    term.add_str(0, 0, query_str);

    term.refresh();
  }

  Status handle_key_event(Ncurses& term, Event const& ev)
  {
    if (ev == Key::Enter) {
      return Status::Selected;
    }
    else if (ev == Key::Esc) {
      return Status::Escaped;
    }
    else if (ev == Key::Up) {
      if (!config.multi_select) {
        choices[cursor + offset].selected = false;
      }
      if (cursor == 0) {
        offset = std::max(0, (int)offset - 1);
      }
      else {
        cursor--;
      }
      if (!config.multi_select) {
        choices[cursor + offset].selected = true;
      }
      return Status::Continue;
    }
    else if (ev == Key::Down) {
      int height;
      std::tie(std::ignore, height) = term.get_size();

      if (!config.multi_select) {
        choices[cursor + offset].selected = false;
      }

      if (cursor == static_cast<size_t>(height - 1 - y_offset)) {
        offset = std::min<size_t>(offset + 1, std::max<int>(0, filtered_len - height + y_offset));
      }
      else {
        cursor = std::min<size_t>(cursor + 1, std::min<size_t>(filtered_len - offset, height - y_offset) - 1);
      }

      if (!config.multi_select) {
        choices[cursor + offset].selected = true;
      }
      return Status::Continue;
    }
    else if (ev == Key::Tab) {
      if (config.multi_select) {
        choices[cursor + offset].selected ^= true;
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
    try {
      auto&& score = regex_score();
      filtered_len = scoring(config.lines, std::move(score), config.score_max);
    }
    catch (std::regex_error&) {
    }

    cursor = 0;
    offset = 0;
    if (!config.multi_select) {
      for (auto& choice : choices) {
        choice.selected = false;
      }
      choices[0].selected = true;
    }
  }

  template <typename Scorer>
  std::size_t scoring(std::vector<std::string> const& lines, Scorer score, std::size_t score_max)
  {
    if (query.empty()) {
      return lines.size();
    }

    for (auto& choice : choices) {
      choice.score = score(lines[choice.index]);
    }
    std::stable_sort(choices.begin(), choices.end());

    return std::find_if(choices.begin(), choices.end(),
                        [score_max](auto& choice) { return choice.score > score_max; }) -
           choices.begin();
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
