/*
===============================================================================

  FILE:  crazyvector_for_pointers.cpp
  
  CONTENTS:
  
    Triangle Strip Compression - GI 2000 demonstration software
  
  PROGRAMMERS:
  
    Martin Isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    Copyright (C) 2000  Martin Isenburg (isenburg@cs.unc.edu)
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    28 June 2000 -- Martin - finalized for GI submission.
  
===============================================================================
*/
#include "crazyvector_for_pointers.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

CrazyVector::CrazyVector()
{
  current_capacity = 1024;
  current_capacity_mask = current_capacity-1;
  data = (const void**) malloc(sizeof(void*)*current_capacity);
  current_size = 0;
  current_begin = 0;
  current_end = 0;
}

CrazyVector::~CrazyVector()
{
  free(data);
}

int CrazyVector::size() const
{
  return current_size;
}

void* CrazyVector::elementAt(int i) const
{
  return (void*)data[((i + current_begin) & current_capacity_mask)];
}

void* CrazyVector::firstElement() const
{
  return (void*)data[current_begin];
}

void CrazyVector::addElement(const void* d) 
{
  if (current_size == current_capacity)
  {
    const void** temp = (const void**) malloc(sizeof(void*)*current_capacity*2);
    if (current_begin < current_end)
    {
      memcpy(temp, &(data[current_begin]), sizeof(void*)*current_size);
    }
    else
    {
      int rest = current_size-current_begin;
      memcpy(temp, &(data[current_begin]), sizeof(void*)*rest);
      memcpy(&(temp[rest]), data, sizeof(void*)*(current_size-rest));
    }
    free(data);
    data = temp;
    current_begin = 0;
    current_end = current_size;
    current_capacity = current_capacity * 2;
    current_capacity_mask = current_capacity - 1;
  }
  data[current_end] = d;
  current_end = (current_end + 1) & current_capacity_mask;
  current_size++;
}

void CrazyVector::lazyRemoveElementAt(int i)
{
  int ri = (i + current_begin)&current_capacity_mask;
  if (i == 0)
  {
    ri = (ri+1)&current_capacity_mask;
    current_size--;
    while ((data[ri] == 0) && current_size)
    {
      ri = (ri+1)&current_capacity_mask;
      current_size--;
    }
    current_begin = ri;
  }
  else
  {
    data[ri] = 0;
  }
}

void CrazyVector::removeFirstElement()
{
  int i = (current_begin+1)&current_capacity_mask;
  current_size--;
  while ((data[i] == 0) && current_size)
  {
    i = (i+1)&current_capacity_mask;
    current_size--;
  }
  current_begin = i;
}
