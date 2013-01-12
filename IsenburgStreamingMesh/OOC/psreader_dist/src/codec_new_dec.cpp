#include "codec_new_dec.h"

#include <stdlib.h>

#include "rangemodel.h"
#include "rangedecoder.h"


TSCcodec::TSCcodec()
{
  ad = 0;
}

TSCcodec::~TSCcodec()
{
}

void TSCcodec::initDec(int vertex_degrees, int pos_bits, int* maxp, int* maxc, FILE* fp)
{
  int i;
  unsigned int table[2] = {1,1};

  h = 0;
  t = 0;
  v = 0;
  s = 0;
  m = 0;

  if (fp)
  {
    ad = new RangeDecoder(fp);
  }

  unsigned int* has_vertex_degree = (unsigned int*)malloc(sizeof(unsigned int)*vertex_degrees);
  // reading bits  that indicate which vertex degrees are there
  for (i = 0; i < vertex_degrees; i++)
  {
    has_vertex_degree[i] = ad->decode(2);
  }

  int has_holes = ad->decode(2);

  int has_nonmanifold = ad->decode(2);

  amAddOrSplitMerge = new RangeModel(2,table,0); // 0: add, 1: split or merge
  amSplitOrMerge = new RangeModel(2,table,0); // 0: split, 1: merge    
  amSplitOffsets = new RangeModel(MODELLED_SPLIT_OFFSETS,0,0); // for cheaper encoding of small offsets
  amVertexDegree = new RangeModel(vertex_degrees,has_vertex_degree,0); // 0: deg2, 1: deg3, 2: deg4, 3: deg5, 4: deg6, 5: deg7, 6: deg8, 7:deg9, 8:deg10 (or bigger)
  amVertexDegreeContinued = new RangeModel(8,0,0); // 0: deg2, 1: deg3, 2: deg4, 3: deg5, 4: deg6, 5: deg7, 6: deg8, 7:deg9, 8:deg10, 9:deg11 (or bigger)
  table[1] = (has_holes ? 1 : 0);
  amPolygonOrHole = new RangeModel(2,table,0); // 0: face, 1: hole
  table[1] = (has_nonmanifold ? 1 : 0);
  amPositionManifoldOrNot = new RangeModel(2,table,0); // 0: manifold, 1: non-manifold
  if (has_nonmanifold)
  {
    amPositionFirstNonManifoldOccurance = new RangeModel(2,0,0); // 0: no, 1: yes 
    amPositionMoreNonManifoldOccurances = new RangeModel(2,0,0); // 0: no, 1: yes 
  }
  else
  {
    amPositionFirstNonManifoldOccurance = 0;
    amPositionMoreNonManifoldOccurances = 0;
  }

  free(has_vertex_degree);

  // geometry

  maxPos[0] = maxp[0];
  maxPos[1] = maxp[1];
  maxPos[2] = maxp[2];

  if (pos_bits == 24)
  {
    // 14-9 = 23+1 bits
    bitsBigLow = 14;
    bitsBigHigh = 9;
    bitsSmall = 12;
  }
  else if (pos_bits == 22)
  {
    // 13-8 = 21+1 bits
    bitsBigLow = 13;
    bitsBigHigh = 8;
    bitsSmall = 11;
  }
  else if (pos_bits == 20)
  {
    // 13-6 = 19+1 bits
    bitsBigLow = 13;
    bitsBigHigh = 6;
    bitsSmall = 10;
  }
  else if (pos_bits == 18)
  {
    // 12-5 = 17+1 bits
    bitsBigLow = 12;
    bitsBigHigh = 5;
    bitsSmall = 9;
  }
  else if (pos_bits == 16)
  {
    // 11-4 = 15+1 bits
    bitsBigLow = 11;
    bitsBigHigh = 4;
    bitsSmall = 8;
  }
  else if (pos_bits == 14)
  {
    // 10-3 = 13+1 bits
    bitsBigLow = 10;
    bitsBigHigh = 3;
    bitsSmall = 7;
  }
  else if (pos_bits == 12)
  {
    // 11 = 11+1 bits
    bitsBigLow = 11;
    bitsBigHigh = 0;
    bitsSmall = 6;
  }
  else if (pos_bits == 10)
  {
    // 9 = 9+1 bits
    bitsBigLow = 9;
    bitsBigHigh = 0;
    bitsSmall = 5;
  }
  else if (pos_bits == 9)
  {
    // 8 = 8+1 bits
    bitsBigLow = 8;
    bitsBigHigh = 0;
    bitsSmall = 5;
  }
  else if (pos_bits == 8)
  {
    // 4 = 7 bits
    bitsBigLow = 7;
    bitsBigHigh = 0;
    bitsSmall = 4;
  }

  bitsMaskBigLow = (1 << bitsBigLow) - 1;
  bitsMaskSmall = (1 << bitsSmall) - 1;

  int absmaxlc = ad->decode((1 << (bitsBigLow+bitsBigHigh)));
  int absmaxac = ad->decode((1 << (bitsBigLow+bitsBigHigh)));

  if (absmaxlc != (1 << (bitsBigLow+bitsBigHigh)) - 1)
  {
//    printf("INFO: the maximal absolute LAST corrector value is %d\n",absmaxlc);
  }
  if (absmaxac != (1 << (bitsBigLow+bitsBigHigh)) - 1)
  {
//    printf("INFO: the maximal absolute ACROSS corrector value is %d\n",absmaxac);
  }
  
  int maxRange;

  maxRange = absmaxlc+1;
  if (maxRange > (1 << bitsBigLow))
  {
    maxRange = (1 << bitsBigLow);
  }
  amBitsLastBigLow = new RangeModel(maxRange,0,0);

  maxRange = absmaxac+1;
  if (maxRange > (1 << bitsBigLow))
  {
    maxRange = (1 << bitsBigLow);
  }
  amBitsAcrossBigLow = new RangeModel(maxRange,0,0);

  if (bitsBigHigh)
  {
    if (absmaxlc >= bitsMaskBigLow)
    {
      amBitsLastBigHigh = (RangeModel**)malloc(sizeof(RangeModel*)*3);
      for (i = 0; i < 3; i++)
      {
        maxRange = (maxc[i] >> bitsBigLow) + 1;
        if (maxRange > (((absmaxlc + 1) >> bitsBigLow) + 1))
        {
          maxRange = (((absmaxlc + 1) >> bitsBigLow) + 1);
        }
        if (maxRange > (1<<bitsBigHigh))
        {
          maxRange = (1<<bitsBigHigh);
        }
        amBitsLastBigHigh[i] = new RangeModel(maxRange,0,0);
      }
    }
    else
    {
      amBitsLastBigHigh = 0;
    }

    if (absmaxac >= bitsMaskBigLow)
    {
      amBitsAcrossBigHigh = (RangeModel**)malloc(sizeof(RangeModel*)*3);
      for (i = 0; i < 3; i++)
      {
        maxRange = (maxc[i] >> bitsBigLow) + 1;
        if (maxRange > (((absmaxac + 1) >> bitsBigLow) + 1))
        {
          maxRange = (((absmaxac + 1) >> bitsBigLow) + 1);
        }
        if (maxRange > (1<<bitsBigHigh))
        {
          maxRange = (1<<bitsBigHigh);
        }
        amBitsAcrossBigHigh[i] = new RangeModel(maxRange,0,0);
      }
    }
    else
    {
      amBitsAcrossBigHigh = 0;
    }
  }
  else
  {
    amBitsLastBigHigh = 0;
    amBitsAcrossBigHigh = 0;
  }

  maxRange = absmaxlc + 1;
  if (maxRange > (1 << bitsSmall))
  {
    maxRange = (1 << bitsSmall);
  }
  amBitsLastSmall = new RangeModel(maxRange,0,0);

  maxRange = absmaxac + 1;
  if (maxRange > (1 << bitsSmall))
  {
    maxRange = (1 << bitsSmall);
  }
  amBitsAcrossSmall = new RangeModel(maxRange,0,0);
}

void TSCcodec::doneDec()
{
  delete ad;

  delete amAddOrSplitMerge;
  delete amSplitOrMerge;
  delete amSplitOffsets;
  delete amVertexDegree;
  delete amPolygonOrHole;
  delete amPositionManifoldOrNot;
  if (amPositionFirstNonManifoldOccurance)
  {
    delete amPositionFirstNonManifoldOccurance;
    delete amPositionMoreNonManifoldOccurances;
  }

  delete amBitsLastBigLow;
  delete amBitsAcrossBigLow;

  if (amBitsLastBigHigh)
  {
    for (int i = 0; i < 3; i++)
    {
      delete amBitsLastBigHigh[i];
    }
    free (amBitsLastBigHigh);
  }

  if (amBitsAcrossBigHigh)
  {
    for (int i = 0; i < 3; i++)
    {
      delete amBitsAcrossBigHigh[i];
    }
    free (amBitsAcrossBigHigh);
  }

  delete amBitsLastSmall;
  delete amBitsAcrossSmall;
}
