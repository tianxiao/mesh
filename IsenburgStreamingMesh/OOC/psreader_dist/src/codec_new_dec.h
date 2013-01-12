/*
===============================================================================

  FILE:  codec_new_dec.h
  
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
  
    19 January 2003 -- Martin - finalized for SIGGRAPH submission.
  
===============================================================================
*/
#ifndef TSC_CODEC_NEW_DEC_H
#define TSC_CODEC_NEW_DEC_H

#include <stdio.h>

#include "rangemodel.h"
#include "rangedecoder.h"

#define ONE_SLOT_RUN 10
//#define ONE_SLOT_RUN 4
#define MODELLED_SPLIT_OFFSETS 50

class TSCcodec
{
public:

  // statistics

  int h;
  int v;
  int t;
  int s;
  int m;

  // arithmetic coding

  RangeDecoder* ad;

  // prob tables for connectivity

  RangeModel* amAddOrSplitMerge;
  RangeModel* amSplitOrMerge;
  RangeModel* amSplitOffsets;
  RangeModel* amVertexDegree;
  RangeModel* amVertexDegreeContinued;
  RangeModel* amPolygonOrHole;
  RangeModel* amPositionManifoldOrNot;
  RangeModel* amPositionFirstNonManifoldOccurance;
  RangeModel* amPositionMoreNonManifoldOccurances;

  // prob tables for geometry

  int maxPos[3];

  RangeModel* amBitsLastSmall;
  RangeModel* amBitsLastBigLow;
  RangeModel** amBitsLastBigHigh;

  RangeModel* amBitsAcrossSmall;
  RangeModel* amBitsAcrossBigLow;
  RangeModel** amBitsAcrossBigHigh;

  int bitsSmall;
  int bitsBigLow;
  int bitsBigHigh;

  int bitsMaskSmall;
  int bitsMaskBigLow;

  void initDec(int, int, int*, int*, FILE*);
  void doneDec();

  inline int readAddOrSplitMerge();
  inline int readSplitOrMerge();
  inline int readPositionManifoldOrNot();
  inline int readPositionFirstNonManifoldOccurance();
  inline int readPositionMoreNonManifoldOccurances();

  inline int readPolygonOrHole();
  inline int readVertexDegree();
  inline int readSplitDirection(int);
  inline int readSplitOffset(int);
  inline int readSplitRemaining(int);
  inline int readMergeIndex(int);
  inline int readMergeDirection(int);
  inline int readMergeOffset(int);
  inline int readMergeRemaining(int);
  inline int readRange(int);

  inline void readPosition(int* pos);
  inline void readPositionCorrectorLast(int* corr);
  inline void readPositionCorrectorAcross(int* corr);
  inline void readPositionCorrector(int* corr, RangeModel* amBitsSmall, RangeModel* amBitsBig);
  inline void readPositionCorrector(int* corr, RangeModel* amBitsSmall, RangeModel* amBitsBigLow, RangeModel** amBitsBigHigh);
  
  TSCcodec();
  ~TSCcodec();
};

inline void TSCcodec::readPosition(int* pos)
{
  pos[0] = ad->decode(maxPos[0]);
  pos[1] = ad->decode(maxPos[1]);
  pos[2] = ad->decode(maxPos[2]);
}

inline void TSCcodec::readPositionCorrectorLast(int* corr)
{
  if (amBitsLastBigHigh)
  {
    readPositionCorrector(corr, amBitsLastSmall, amBitsLastBigLow, amBitsLastBigHigh);
  }
  else
  {
    readPositionCorrector(corr, amBitsLastSmall, amBitsLastBigLow);
  }
}

inline void TSCcodec::readPositionCorrectorAcross(int* corr)
{
  if (amBitsAcrossBigHigh)
  {
    readPositionCorrector(corr, amBitsAcrossSmall, amBitsAcrossBigLow, amBitsAcrossBigHigh);
  }
  else
  {
    readPositionCorrector(corr, amBitsAcrossSmall, amBitsAcrossBigLow);
  }
}

inline void TSCcodec::readPositionCorrector(int* corr, RangeModel* amBitsSmall, RangeModel* amBitsBig)
{
  int i, c;
  for (i = 0; i < 3; i++)
  {
    c = ad->decode(amBitsSmall);
    if (c == bitsMaskSmall)
    {
      c = ad->decode(amBitsBig);
    }
    if (c != 0) // decode sign
    {
      if (ad->decode(2) == 1)
      {
        c = -c;
      }
    }
    corr[i] = c;
  }
}

inline void TSCcodec::readPositionCorrector(int* corr, RangeModel* amBitsSmall, RangeModel* amBitsBigLow, RangeModel** amBitsBigHigh)
{
  int i, c;
  for (i = 0; i < 3; i++)
  {
    c = ad->decode(amBitsSmall);
    if (c == bitsMaskSmall)
    {
      c = ad->decode(amBitsBigHigh[i]) << bitsBigLow;
      c |= ad->decode(amBitsBigLow);
    }
    if (c != 0) // decode sign
    {
      if (ad->decode(2) == 1)
      {
        c = -c;
      }
    }
    corr[i] = c;
  }
}

inline int TSCcodec::readAddOrSplitMerge()
{
  return ad->decode(amAddOrSplitMerge);
}

inline int TSCcodec::readSplitOrMerge()
{
  return ad->decode(amSplitOrMerge);
}

inline int TSCcodec::readPositionManifoldOrNot()
{
  return ad->decode(amPositionManifoldOrNot);
}

inline int TSCcodec::readPositionFirstNonManifoldOccurance()
{
  return ad->decode(amPositionFirstNonManifoldOccurance);
}

inline int TSCcodec::readPositionMoreNonManifoldOccurances()
{
  return ad->decode(amPositionMoreNonManifoldOccurances);
}

inline int TSCcodec::readPolygonOrHole()
{
  return ad->decode(amPolygonOrHole);
}

inline int TSCcodec::readVertexDegree()
{
  v++;
  int degree = 2 + ad->decode(amVertexDegree);
  if (degree == amVertexDegree->n + 1)
  {
    int continued;
    do
    {
      continued = ad->decode(amVertexDegreeContinued);
      degree += continued;
    } while (continued == amVertexDegreeContinued->n-1);
  }
  return degree;
}

inline int TSCcodec::readSplitDirection(int range)
{
  s++;
  return ad->decode(range);
}

inline int TSCcodec::readSplitOffset(int range)
{
  int offset = ad->decode(amSplitOffsets);
  if (offset == (MODELLED_SPLIT_OFFSETS-1))
  {
    return ad->decode(range-(MODELLED_SPLIT_OFFSETS-1))+(MODELLED_SPLIT_OFFSETS-1);
  }
  else
  {
    return offset;
  }
}

inline int TSCcodec::readSplitRemaining(int range)
{
  return ad->decode(range);
}

inline int TSCcodec::readMergeIndex(int range)
{
  return ad->decode(range);
}

inline int TSCcodec::readMergeDirection(int range)
{
  m++;
  return ad->decode(range);
}

inline int TSCcodec::readMergeOffset(int range)
{
  return ad->decode(range);
}

inline int TSCcodec::readMergeRemaining(int range)
{
  return ad->decode(range);
}

inline int TSCcodec::readRange(int range)
{
  return ad->decode(range);
}

#endif
