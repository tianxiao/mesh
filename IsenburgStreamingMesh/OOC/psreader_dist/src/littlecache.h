/*
===============================================================================

  FILE:  littlecache.h

  CONTENTS:


  PROGRAMMERS:

    martin isenburg@cs.unc.edu

  COPYRIGHT:

    copyright (C) 2003  martin isenburg@cs.unc.edu

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  CHANGE HISTORY:

    07 October 2003 -- initial version created the day of the California recall

===============================================================================
*/
#ifndef LITTLE_CACHE_H
#define LITTLE_CACHE_H

class LittleCache
{
public:
  LittleCache();
  ~LittleCache();

  inline void put(int i0, int i1, int i2);
  inline void put(void* d0, void* d1, void* d2);
  inline void put(int i0, int i1, int i2, void* d0, void* d1, void* d2);
  inline void* get(int i);
  inline int pos(int i);

private:  
  int index[3];
  void* data[3];
};

inline LittleCache::LittleCache()
{
  index[0] = -1;
  index[1] = -1;
  index[2] = -1;
  data[0] = 0;
  data[1] = 0;
  data[2] = 0;
}

inline LittleCache::~LittleCache()
{
}

inline void LittleCache::put(int i0, int i1, int i2)
{
  index[0] = i0;
  index[1] = i1;
  index[2] = i2;
}

inline void LittleCache::put(void* d0, void* d1, void* d2)
{
  data[0] = d0;
  data[1] = d1;
  data[2] = d2;
}

inline void LittleCache::put(int i0, int i1, int i2, void* d0, void* d1, void* d2)
{
  index[0] = i0;
  index[1] = i1;
  index[2] = i2;
  data[0] = d0;
  data[1] = d1;
  data[2] = d2;
}

inline void* LittleCache::get(int i)
{
  return data[i];
}

inline int LittleCache::pos(int i)
{
  if (i == index[0])
  {
    return 0;
  }
  if (i == index[1])
  {
    return 1;
  }
  if (i == index[2])
  {
    return 2;
  }
  return -1;
}

#endif
