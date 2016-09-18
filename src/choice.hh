#ifndef __HEADER_CHOICE__
#define __HEADER_CHOICE__

#include <cstdint>

struct Choice {
  std::size_t index;
  double score = 0;
  bool selected = false;

public:
  Choice() = default;
  Choice(std::size_t index) : index(index) {}

  bool operator>(Choice const& rhs) const { return score > rhs.score; }
};

#endif