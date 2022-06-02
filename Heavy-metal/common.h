#ifndef COMMON_H
#define COMMON_H

struct block_range
{
  ssize_t start;
  ssize_t end;
  struct block_range* pNext;

};

#endif // COMMON_H
