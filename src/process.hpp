#pragma once

#include <string>
#include <vector>

class process
{

public:
  process(const std::vector<std::wstring> &argv);
  process(const std::vector<std::string> &argv);

  // Dissallow copying so we only close fd's once
  process(const process &other) = delete;
  process &operator=(const process &other) = delete;

  process(process &&other) noexcept;
  process &operator=(process &&other) noexcept;

  ~process();

  bool finished() const;

  void write(const char *string);

private:
  struct impl;
  impl *pimpl;
};

process &operator<<(process &process, const char *string);