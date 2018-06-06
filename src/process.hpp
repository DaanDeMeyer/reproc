#pragma once

#include <memory>
#include <string>

class process
{

public:
  process(const std::string &path, char *const argv);

  // Dissallow copying so we only close fd's once
  process(const process &other) = delete;
  process &operator=(const process &other) = delete;

  process(process &&other) noexcept;
  process &operator=(process &&other) noexcept;

  ~process();

  uint32_t pid() const;

  bool finished() const;

  void write(const std::string &string);

private:
  uint32_t _pid = 0; // 0 stands for process not spawned

  struct impl;
  std::unique_ptr<impl> pimpl;
};

process &operator<<(process &process, const std::string &string);