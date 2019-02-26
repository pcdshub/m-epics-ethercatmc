/*
  FILENAME... EthercatMCHelper.cpp
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>

#include "EthercatMC.h"
#include "EthercatMCIndexerAxis.h"

#include <epicsThread.h>

static unsigned indexGroup = 0x4020;

/* Parameter interface */
/* The highest 3 bits are used for the command itself */
#define PARAM_IF_CMD_MASK                          0xE000

#define PARAM_IF_CMD_INVALID                       0x0000
#define PARAM_IF_CMD_DOREAD                        0x2000
#define PARAM_IF_CMD_DOWRITE                       0x4000
#define PARAM_IF_CMD_BUSY                          0x6000
#define PARAM_IF_CMD_DONE                          0x8000
#define PARAM_IF_CMD_ERR_NO_IDX                    0xA000
#define PARAM_IF_CMD_READONLY                      0xC000
#define PARAM_IF_CMD_RETRY_LATER                   0xE000

#define MAX_ADSPORT 853

#ifndef ASYN_TRACE_INFO
#define ASYN_TRACE_INFO      0x0040
#endif

extern "C" {
  const char *plcUnitTxtFromUnitCode(unsigned unitCode)
  {
    const static char *const unitTxts[] = {
      " ",
      "V",
      "A",
      "W",
      "m",
      "gr",
      "Hz",
      "T",
      "K",
      "C",
      "F",
      "bar",
      "degree",
      "Ohm",
      "m/sec",
      "m2/sec",
      "m3/sec",
      "s",
      "counts",
      "bar/sec",
      "bar/sec2",
      "F",
      "H" };
    if (unitCode < sizeof(unitTxts)/sizeof(unitTxts[0]))
      return unitTxts[unitCode];

    switch (unitCode) {
    case 0xfd04:
      return "mm";
    default:
      return "unit";
    }
  }
};
/*
  1   2   3   4   5   6   7   8   9   10   11   12       13   14   15     16      17  18       19      20       21  22
  V   A   W   m   g   Hz  T   K   °C   °F  bar   °(deg)  Ohm  m/s   m²/s   m³/s   s   counts   bar/s   bar/s²   F   H

  Beispiele für zusammengesetzte Einheiten:

*/

asynStatus EthercatMCController::writeReadControllerPrint(int traceMask)
{
  asynStatus status = writeReadOnErrorDisconnect();
  if (status) traceMask |= ASYN_TRACE_ERROR|ASYN_TRACEIO_DRIVER;
  asynPrint(pasynUserController_, traceMask,
            "%sout=%s in=%s status=%s (%d)\n",
            modNamEMC, outString_, inString_,
            pasynManager->strStatus(status), (int)status);
  return status;
}

asynStatus EthercatMCController::getPlcMemoryUint(unsigned indexOffset,
                                                  unsigned *value,
                                                  size_t lenInPlc)
{

  int traceMask = 0;
  int nvals;
  int iRes;
  asynStatus status;

  if (lenInPlc == 2) {
    snprintf(outString_, sizeof(outString_),
             "ADSPORT=%u/.ADR.16#%X,16#%X,2,18?",
             adsport,
             indexGroup, indexOffset);
  } else if (lenInPlc == 4) {
    snprintf(outString_, sizeof(outString_),
             "ADSPORT=%u/.ADR.16#%X,16#%X,4,19?",
             adsport,
             indexGroup, indexOffset);
  } else if (lenInPlc == 8) {
    snprintf(outString_, sizeof(outString_),
             "ADSPORT=%u/.ADR.16#%X,16#%X,8,21?",
             adsport,
             indexGroup, indexOffset);
  } else {
    return asynError;
  }
  status = writeReadControllerPrint(traceMask);
  if (status) return asynError;
  nvals = sscanf(inString_, "%u", &iRes);
  if (nvals == 1) {
    *value = iRes;
    asynPrint(pasynUserController_, traceMask,
              "%sout=%s in=%s iRes=0x%x\n",
              modNamEMC, outString_, inString_, iRes);
    return asynSuccess;
  }

  asynPrint(pasynUserController_, ASYN_TRACE_ERROR|ASYN_TRACEIO_DRIVER,
            "%snvals=%d command=\"%s\" response=\"%s\"\n",
            modNamEMC, nvals, outString_, inString_);
  return asynDisabled;
}

asynStatus EthercatMCController::getPlcMemorySint(unsigned indexOffset,
                                                  int *value,
                                                  size_t lenInPlc)
{
  int traceMask = 0;
  int nvals;
  int iRes;
  asynStatus status;

  if (lenInPlc == 2) {
    snprintf(outString_, sizeof(outString_),
             "ADSPORT=%u/.ADR.16#%X,16#%X,2,2?",
             adsport,
             indexGroup, indexOffset);
  } else if (lenInPlc == 4) {
    snprintf(outString_, sizeof(outString_),
             "ADSPORT=%u/.ADR.16#%X,16#%X,4,3?",
             adsport,
             indexGroup, indexOffset);
  } else if (lenInPlc == 8) {
    snprintf(outString_, sizeof(outString_),
             "ADSPORT=%u/.ADR.16#%X,16#%X,8,20?",
             adsport,
             indexGroup, indexOffset);
  } else {
    return asynError;
  }
  status = writeReadControllerPrint(traceMask);
  if (status) return status;

  nvals = sscanf(inString_, "%d", &iRes);
  if (nvals == 1) {
    *value = iRes;
    return asynSuccess;
  }

  asynPrint(pasynUserController_, ASYN_TRACE_ERROR|ASYN_TRACEIO_DRIVER,
            "%snvals=%d command=\"%s\" response=\"%s\"\n",
            modNamEMC, nvals, outString_, inString_);
  return asynDisabled;
}

asynStatus EthercatMCController::getPlcMemoryString(unsigned indexOffset,
                                                         char *value,
                                                         size_t len)
{
  int traceMask = 0;
  asynStatus status;

  snprintf(outString_, sizeof(outString_),
           "ADSPORT=%u/.ADR.16#%X,16#%X,%d,30?",
           adsport,
           indexGroup, indexOffset, (int)len);
  status = writeReadControllerPrint(traceMask);
  if (status) {
    asynPrint(pasynUserController_, ASYN_TRACE_ERROR|ASYN_TRACEIO_DRIVER,
              "%scommand=\"%s\" response=\"%s\"\n",
              modNamEMC, outString_, inString_);
    return asynError;
  }

  memcpy(value, inString_, len);
  return asynSuccess;
}

asynStatus EthercatMCController::setPlcMemoryInteger(unsigned indexOffset,
                                                     int value,
                                                     size_t lenInPlc)
{
  int traceMask = 0;
  asynStatus status;
  if (lenInPlc == 2) {
    snprintf(outString_, sizeof(outString_),
             "ADSPORT=%u/.ADR.16#%X,16#%X,2,2=%d",
             adsport,
             indexGroup, indexOffset, value);
  } else if (lenInPlc == 4) {
    snprintf(outString_, sizeof(outString_),
             "ADSPORT=%u/.ADR.16#%X,16#%X,4,3=%d",
             adsport,
             indexGroup, indexOffset, value);
  } else if (lenInPlc == 8) {
    snprintf(outString_, sizeof(outString_),
             "ADSPORT=%u/.ADR.16#%X,16#%X,8,20=%d",
             adsport,
             indexGroup, indexOffset, value);
  } else {
    return asynError;
  }
  status = writeReadOnErrorDisconnect();
  if (status) traceMask |= ASYN_TRACE_ERROR|ASYN_TRACEIO_DRIVER;
  asynPrint(pasynUserController_, traceMask,
            "%sout=%s in=%s status=%s (%d)\n",
            modNamEMC, outString_, inString_,
            pasynManager->strStatus(status), (int)status);
  if (status) return status;
  return checkACK(outString_, strlen(outString_), inString_);
}


asynStatus EthercatMCController::getPlcMemoryDouble(unsigned indexOffset,
                                                    double *value,
                                                    size_t lenInPlc)
{
  int traceMask = 0;
  int nvals;
  double fRes;
  asynStatus status;

  if (lenInPlc == 4) {
    snprintf(outString_, sizeof(outString_),
             "ADSPORT=%u/.ADR.16#%X,16#%X,4,4?",
             adsport,
             indexGroup, indexOffset);
  } else if (lenInPlc == 8) {
    snprintf(outString_, sizeof(outString_),
             "ADSPORT=%u/.ADR.16#%X,16#%X,8,5?",
             adsport,
             indexGroup, indexOffset);
  } else {
    return asynError;
  }
  status = writeReadControllerPrint(traceMask);
  if (status) return status;

  nvals = sscanf(inString_, "%lf", &fRes);
  if (nvals == 1) {
    *value = fRes;
    return asynSuccess;
  }

  asynPrint(pasynUserController_, ASYN_TRACE_ERROR|ASYN_TRACEIO_DRIVER,
            "%snvals=%d command=\"%s\" response=\"%s\"\n",
            modNamEMC, nvals, outString_, inString_);
  return asynDisabled;
}

asynStatus EthercatMCController::setPlcMemoryDouble(unsigned indexOffset,
                                                    double value,
                                                    size_t lenInPlc)
{
  asynStatus status;

  if (lenInPlc == 4) {
    snprintf(outString_, sizeof(outString_),
             "ADSPORT=%u/.ADR.16#%X,16#%X,4,4=%f",
             adsport,
             indexGroup, indexOffset, value);
  } else if (lenInPlc == 8) {
    snprintf(outString_, sizeof(outString_),
             "ADSPORT=%u/.ADR.16#%X,16#%X,8,5=%f",
             adsport,
             indexGroup, indexOffset, value);
  } else {
    return asynError;
  }
  status = writeReadOnErrorDisconnect();
  if (status) return status;
  return checkACK(outString_, strlen(outString_), inString_);
}


asynStatus EthercatMCController::getPlcMemoryBytes(unsigned indexOffset,
                                                   unsigned char *value,
                                                   size_t lenInPlc)
{
  int nvals;
  unsigned res;
  asynStatus status;

  if (adsport) {
    if (lenInPlc) {
      snprintf(outString_, sizeof(outString_),
               "ADSPORT=%u/.ADR.16#%X,16#%X,4,4?",
               adsport,
               indexGroup, indexOffset);
      status = writeReadOnErrorDisconnect();
      if (status) return status;
      nvals = sscanf(inString_, "%u", &res);
      asynPrint(pasynUserController_, ASYN_TRACE_ERROR|ASYN_TRACEIO_DRIVER,
                "%snvals=%d command=\"%s\" response=\"%s\"\n",
                modNamEMC, nvals, outString_, inString_);
    }
    if (lenInPlc) {
      snprintf(outString_, sizeof(outString_),
               "ADSPORT=%u/.ADR.16#%X,16#%X,4,19?",
               adsport,
               indexGroup, indexOffset);
      status = writeReadOnErrorDisconnect();
      if (status) return status;
      nvals = sscanf(inString_, "%u", &res);
      asynPrint(pasynUserController_, ASYN_TRACE_ERROR|ASYN_TRACEIO_DRIVER,
                "%snvals=%d command=\"%s\" response=\"%s\"\n",
                modNamEMC, nvals, outString_, inString_);
    }
    if (lenInPlc) {
      snprintf(outString_, sizeof(outString_),
               "ADSPORT=%u/.ADR.16#%X,16#%X,2,2?",
               adsport,
               indexGroup, indexOffset);
      status = writeReadOnErrorDisconnect();
      if (status) return status;
      nvals = sscanf(inString_, "%u", &res);
      asynPrint(pasynUserController_, ASYN_TRACE_ERROR|ASYN_TRACEIO_DRIVER,
                "%snvals=%d command=\"%s\" response=\"%s\"\n",
                modNamEMC, nvals, outString_, inString_);
    }


    if (lenInPlc) {
      int traceMask = 0;
      snprintf(outString_, sizeof(outString_),
               "ADSPORT=%u/.ADR.16#%X,16#%X,%u,17?",
               adsport,
               indexGroup, indexOffset, (unsigned)lenInPlc);
      status = writeReadControllerPrint(traceMask);
      if (status) return status;
      nvals = sscanf(inString_, "%u", &res);
      asynPrint(pasynUserController_, ASYN_TRACE_ERROR|ASYN_TRACEIO_DRIVER,
                "%snvals=%d command=\"%s\" response=\"%s\"\n",
                modNamEMC, nvals, outString_, inString_);
      asynPrint(pasynUserController_, ASYN_TRACE_INFO,
                "%sout=%s in=%s res=0x%x\n",
                modNamEMC, outString_, inString_, res);
      *value = res;
      lenInPlc--;
      value++;
      indexOffset++;
    }



    while (lenInPlc) {
      snprintf(outString_, sizeof(outString_),
               "ADSPORT=%u/.ADR.16#%X,16#%X,1,17?",
               adsport,
               indexGroup, indexOffset);
      status = writeReadOnErrorDisconnect();
      if (status) return status;
      nvals = sscanf(inString_, "%u", &res);
      asynPrint(pasynUserController_, ASYN_TRACE_ERROR|ASYN_TRACEIO_DRIVER,
                "%snvals=%d command=\"%s\" response=\"%s\"\n",
                modNamEMC, nvals, outString_, inString_);
      asynPrint(pasynUserController_, ASYN_TRACE_INFO,
                "%sout=%s in=%s res=0x%x\n",
                modNamEMC, outString_, inString_, res);
      *value = res;
      lenInPlc--;
      value++;
      indexOffset++;
    }
  }
  return asynSuccess;
}

asynStatus EthercatMCController::readDeviceIndexer(unsigned indexOffset,
                                                   unsigned devNum,
                                                   unsigned infoType)
{
  asynStatus status;
  unsigned value = (devNum + (infoType << 8));
  unsigned valueAcked = 0x8000 + value;
  unsigned counter = 0;
  if (devNum > 0xFF)   return asynDisabled;
  if (infoType > 0xFF) return asynDisabled;

  /* https://forge.frm2.tum.de/public/doc/plc/master/singlehtml/
     The ACK bit on bit 15 must be set when we read back.
     devNum and infoType must match our request as well,
     otherwise there is a collision.
  */
  status = setPlcMemoryInteger(indexOffset, value, 2);
  if (status) return status;
  while (counter < 5) {
    status = getPlcMemoryUint(indexOffset, &value, 2);
    if (status) return status;
    if (value == valueAcked) return asynSuccess;
    counter++;
    epicsThreadSleep(.1 * (counter<<1));
  }
  return asynDisabled;
}

asynStatus EthercatMCController::indexerParamWaitNotBusy(unsigned indexOffset)
{
  asynStatus status;
  unsigned   cmdSubParamIndex = 0;
  unsigned   counter = 0;

  while (counter < 5) {
    unsigned traceMask = 0;
    status = getPlcMemoryUint(indexOffset, &cmdSubParamIndex, 2);
    if (status) traceMask |= ASYN_TRACE_ERROR|ASYN_TRACEIO_DRIVER;
    asynPrint(pasynUserController_, traceMask,
              "%sout=%s in=%s cmdSubParamIndex=0x%04x status=%s (%d)\n",
              modNamEMC, outString_, inString_, cmdSubParamIndex,
              pasynManager->strStatus(status), (int)status);
    if (status) return status;
    switch (cmdSubParamIndex & PARAM_IF_CMD_MASK) {
      case PARAM_IF_CMD_INVALID:
      case PARAM_IF_CMD_DONE:
      case PARAM_IF_CMD_ERR_NO_IDX:
      case PARAM_IF_CMD_READONLY:
        return asynSuccess;
      case PARAM_IF_CMD_RETRY_LATER:
        return asynDisabled;
      default:
        ; /* Read, write or busy. continue looping */
    }
    counter++;
    epicsThreadSleep(.1 * (counter<<1));
  }
  return asynDisabled;
}


asynStatus EthercatMCController::indexerPrepareParamRead(unsigned indexOffset,
                                                         unsigned paramIndex)
{
  int traceMask = 0;
  asynStatus status;
  unsigned cmdSubParamIndex;
  unsigned cmd      = 0x2000 + paramIndex;
  unsigned cmdAcked = 0x8000 + paramIndex;
  unsigned counter = 0;

  if (paramIndex > 0x7F) return asynDisabled;
  status = indexerParamWaitNotBusy(indexOffset);
  if (status) return status;

  status = setPlcMemoryInteger(indexOffset, cmd, 2);
  if (status) return status;
  while (counter < 5) {
    status = getPlcMemoryUint(indexOffset, &cmdSubParamIndex, 2);
    asynPrint(pasynUserController_, traceMask,
              "%sout=%s in=%s status=%s (%d)\n",
              modNamEMC, outString_, inString_,
              pasynManager->strStatus(status), (int)status);
    if (status) return status;
    switch (cmdSubParamIndex >> 13) {
      case 1:
      case 2:
      case 3:
        continue;
      case 0:
      case 4:
      case 5:
      case 6:
      case 7:
        break;
    }
    if (cmdSubParamIndex == cmdAcked) return asynSuccess;
    counter++;
    epicsThreadSleep(.1 * (counter<<1));
  }
  return asynDisabled;
}

asynStatus EthercatMCController::indexerParamWrite(unsigned paramIfOffset,
                                                   unsigned paramIndex,
                                                   double value)
{
  unsigned traceMask = ASYN_TRACE_INFO;
  asynStatus status;
  unsigned cmd      = PARAM_IF_CMD_DOWRITE + paramIndex;
  unsigned cmdAcked = PARAM_IF_CMD_DONE    + paramIndex;
  size_t lenInPlcCmd = 2;
  size_t lenInPlcPara = 4;
  unsigned counter = 0;

  if (paramIndex > 0x7F) return asynDisabled;
  status = indexerParamWaitNotBusy(paramIfOffset);
  if (status) return status;

  /*
     The parameter interface has this layout:
     0 CmdParamReasonIdx
     2 ParamValue
  */
  /* Parameters 1..4 are integers, the rest is floating point */
  if (paramIndex <= 4)
    status = setPlcMemoryInteger(paramIfOffset + 2, (int)value, lenInPlcPara);
  else
    status = setPlcMemoryDouble(paramIfOffset + 2, value, lenInPlcPara);
  if (status) traceMask |= ASYN_TRACE_ERROR|ASYN_TRACEIO_DRIVER;
  asynPrint(pasynUserController_, traceMask,
            "%sout=%s in=%s status=%s (%d)\n",
            modNamEMC, outString_, inString_,
            pasynManager->strStatus(status), (int)status);
  if (status) return status;

  status = setPlcMemoryInteger(paramIfOffset, cmd, lenInPlcCmd);
  if (status) traceMask |= ASYN_TRACE_ERROR|ASYN_TRACEIO_DRIVER;
  asynPrint(pasynUserController_, traceMask,
            "%sout=%s in=%s status=%s (%d)\n",
            modNamEMC, outString_, inString_,
            pasynManager->strStatus(status), (int)status);
  if (status) return status;
  while (counter < 5) {
    unsigned cmdSubParamIndex = 0;
    status = getPlcMemoryUint(paramIfOffset, &cmdSubParamIndex, 2);
    if (status) traceMask |= ASYN_TRACE_ERROR|ASYN_TRACEIO_DRIVER;
    asynPrint(pasynUserController_, traceMask,
              "%sout=%s in=%s cmdSubParamIndex=0x%04x counter=%u status=%s (%d)\n",
              modNamEMC, outString_, inString_, cmdSubParamIndex,
              counter,
              pasynManager->strStatus(status), (int)status);
    if (status) return status;
    /* This is good, return */
    if (cmdSubParamIndex == cmdAcked) return asynSuccess;
    switch (cmdSubParamIndex & PARAM_IF_CMD_MASK) {
      case PARAM_IF_CMD_INVALID:
        status = asynDisabled;
      case PARAM_IF_CMD_DOREAD:
        status = asynDisabled;
      case PARAM_IF_CMD_DOWRITE:
      case PARAM_IF_CMD_BUSY:
        break;
      case PARAM_IF_CMD_DONE:
        /* This is an error. (collision ?) */
        status = asynDisabled;
      case PARAM_IF_CMD_ERR_NO_IDX:
        status = asynDisabled;
      case PARAM_IF_CMD_READONLY:
        status = asynDisabled;
      case PARAM_IF_CMD_RETRY_LATER:
        status = asynDisabled;
    }
    if (status) {
      traceMask |= ASYN_TRACE_ERROR|ASYN_TRACEIO_DRIVER;
      asynPrint(pasynUserController_, traceMask,
                "%sout=%s in=%s cmdSubParamIndex=0x%04x counter=%u status=%s (%d)\n",
                modNamEMC, outString_, inString_, cmdSubParamIndex,
                counter,
                pasynManager->strStatus(status), (int)status);
      return status;
    }
    epicsThreadSleep(.1 * (counter<<1));
    counter++;
  }
  return asynDisabled;
}


asynStatus EthercatMCController::poll(void)
{
  asynStatus status = asynSuccess;
  if (!ctrlLocal.initialPollDone) {
    status = initialPollIndexer();
    if (!status) ctrlLocal.initialPollDone = 1;
  }
  return status;
}

asynStatus EthercatMCController::initialPollIndexer(void)
{
  asynStatus status;
  double version = 0.0;
  unsigned indexerOffset = 0;
  struct {
    char desc[34];
    char vers[34];
    char author1[34];
    char author2[34];
  } descVersAuthors;
  unsigned devNum;
  unsigned infoType0 = 0;
  unsigned infoType4 = 4;
  unsigned infoType5 = 5;
  unsigned infoType6 = 6;
  unsigned infoType7 = 7;
  unsigned infoType15 = 15;
  unsigned infoType16 = 16;
  int      axisNo = 0;

  memset(&descVersAuthors, 0, sizeof(descVersAuthors));
  adsport = 851;
  while ((adsport < MAX_ADSPORT) && !version) {
    double tmp_version;
    unsigned int iTmpVer = 0;
    size_t lenInPlc = 4;
    getPlcMemoryUint(indexerOffset, &iTmpVer, lenInPlc);
    status = getPlcMemoryDouble(indexerOffset, &tmp_version, lenInPlc);
    asynPrint(pasynUserController_, ASYN_TRACE_INFO,
              "%sadsport=%u iTmpVer=0x%08x indexerVersion=%f\n",
              modNamEMC, adsport, iTmpVer, tmp_version);
    if ((tmp_version == 2015.02) || (tmp_version == 2015.020020)) {
      version = tmp_version;
    } else {
      adsport++;
    }
  }
  if (!version) status = asynDisabled;
  if (status) goto endPollIndexer;

  indexerOffset = 4;
  status = getPlcMemoryUint(indexerOffset, &indexerOffset, 2);
  asynPrint(pasynUserController_, ASYN_TRACE_INFO,
            "%sindexerOffset=%u\n",
            modNamEMC, indexerOffset);

  for (devNum = 0; devNum < 100; devNum++) {
    unsigned iTypCode = -1;
    unsigned iSize = -1;
    unsigned iOffset = -1;
    unsigned iUnit = -1;
    unsigned iAllFlags = -1;
    double fAbsMin = 0;
    double fAbsMax = 0;
    status = readDeviceIndexer(indexerOffset, devNum, infoType0);
    if (!status) {
      unsigned iFlagsLow = -1;
      unsigned iFlagsHigh = -1;
      getPlcMemoryUint(indexerOffset +  1*2, &iTypCode, 2);
      getPlcMemoryUint(indexerOffset +  2*2, &iSize, 2);
      getPlcMemoryUint(indexerOffset +  3*2, &iOffset, 2);
      getPlcMemoryUint(indexerOffset +  4*2, &iUnit, 2);
      getPlcMemoryUint(indexerOffset +  5*2, &iFlagsLow, 2);
      getPlcMemoryUint(indexerOffset +  6*2, &iFlagsHigh, 2);
      getPlcMemoryDouble(indexerOffset  +  7*2, &fAbsMin, 4);
      getPlcMemoryDouble(indexerOffset  +  9*2, &fAbsMax, 4);
      iAllFlags = iFlagsLow + (iFlagsHigh << 16);
    }
    status = readDeviceIndexer(indexerOffset, devNum, infoType4);
    if (!status) {
      getPlcMemoryString(indexerOffset + 1*2,
                              descVersAuthors.desc,
                              sizeof(descVersAuthors.desc));
    }
    status = readDeviceIndexer(indexerOffset, devNum, infoType5);
    if (!status) {
      getPlcMemoryString(indexerOffset + 1*2,
                              descVersAuthors.vers,
                              sizeof(descVersAuthors.vers));
    }
    status = readDeviceIndexer(indexerOffset, devNum, infoType6);
    if (!status) {
      getPlcMemoryString(indexerOffset + 1*2,
                              descVersAuthors.author1,
                              sizeof(descVersAuthors.author1));
    }
    status = readDeviceIndexer(indexerOffset, devNum, infoType7);
    if (!status) {
      getPlcMemoryString(indexerOffset + 1*2,
                              descVersAuthors.author2,
                              sizeof(descVersAuthors.author2));
    }
    asynPrint(pasynUserController_, ASYN_TRACE_INFO,
              "%sindexerRequest %20s "
              "TypCode=0x%x Size=%u Offset=%u Unit=%s (0x%x) AllFlags=0x%x AbsMin=%e AbsMax=%e\n",
              modNamEMC, descVersAuthors.desc,
              iTypCode, iSize, iOffset, plcUnitTxtFromUnitCode(iUnit),
              iUnit, iAllFlags, fAbsMin, fAbsMax);
    asynPrint(pasynUserController_, ASYN_TRACE_INFO,
              "%sdescVersAuthors(%d)  vers=%s author1=%s author2=%s\n",
              modNamEMC, axisNo,
              descVersAuthors.vers,
              descVersAuthors.author1,
              descVersAuthors.author2);
    {
      unsigned auxBitIdx = 0;
      for (auxBitIdx = 0; auxBitIdx < 23; auxBitIdx++) {
        if ((iAllFlags >> auxBitIdx) & 1) {
          char auxBitName[34];
          memset(&auxBitName, 0, sizeof(auxBitName));
          status = readDeviceIndexer(indexerOffset, devNum, infoType16 + auxBitIdx);
          if (!status) {
            getPlcMemoryString(indexerOffset + 1*2,
                                    auxBitName,
                                    sizeof(auxBitName));
            asynPrint(pasynUserController_, ASYN_TRACE_INFO,
                      "%sauxBitName(%d) auxBitName(%02u)=%s\n",
                      modNamEMC, axisNo, auxBitIdx, auxBitName);
          }
        }
      }
    }

    if (!iTypCode && !iSize && !iOffset) {
      break; /* End of list ?? */
    }
    status = readDeviceIndexer(indexerOffset, devNum, infoType15);
    if (!status) {
      unsigned dataIdx;
      unsigned parameters;
      /*  motors */
      switch (iTypCode) {
      case 0x5008:
      case 0x500C:
        axisNo++;
        {
          EthercatMCIndexerAxis *pAxis = static_cast<EthercatMCIndexerAxis*>(asynMotorController::getAxis(axisNo));
          if (!pAxis) {
            pAxis = new EthercatMCIndexerAxis(this, axisNo);
          }
          asynPrint(pasynUserController_, ASYN_TRACE_INFO,
                    "%sTypeCode(%d) iTypCode=%x pAxis=%p\n",
                    modNamEMC, axisNo, iTypCode, pAxis);
          if (pAxis) {
            int validSoftlimits = 0; // fAbsMax > fAbsMin;

            if (fAbsMin <= -3.402824e+38 && fAbsMax >= 3.402822e+38)
              validSoftlimits = 0;

            pAxis->setIndexerTypeCodeOffset(iTypCode, iOffset);
            setStringParam(axisNo,  EthercatMCCfgDESC_RB_, descVersAuthors.desc);
            setStringParam(axisNo,  EthercatMCCfgEGU_RB_, plcUnitTxtFromUnitCode(iUnit));
            /* Soft limits */
            setIntegerParam(axisNo, EthercatMCCfgDHLM_En_, validSoftlimits);
            setDoubleParam(axisNo,  EthercatMCCfgDHLM_,    fAbsMax);
            setIntegerParam(axisNo, EthercatMCCfgDLLM_En_, validSoftlimits);
            setDoubleParam(axisNo,  EthercatMCCfgDLLM_,    fAbsMin);
          }
        }
        break;
      default:
        ;
      }
      for (dataIdx = 0; dataIdx <= 16; dataIdx++) {
        parameters = -1;
        status = getPlcMemoryUint(indexerOffset + (1 + dataIdx) * 2, &parameters, 2);
        if (!status) {
          int traceMask = 0;
          /* dataIdx == 0 has ACK + infoType/devNum
             dataIdx == 1 has supported parameters 15..0 */
          asynPrint(pasynUserController_, traceMask,
                    "%sparameters[%03u..%03u]=0x%04x\n",
                    modNamEMC, dataIdx*16 +15,
                    dataIdx*16, parameters);
          /* param devices */
          switch (iTypCode) {
            case 0x5008:
            case 0x500C:
              {
                EthercatMCIndexerAxis *pAxis = static_cast<EthercatMCIndexerAxis*>(asynMotorController::getAxis(axisNo));
                unsigned regSize = 2;
                unsigned indexOffset = iOffset + 5*regSize;
                unsigned bitIdx;
                double microsteps = 1.0;  /* default */
                double fullsrev = 200;    /* (default) Full steps/revolution */

                for (bitIdx = 0; bitIdx <= 15; bitIdx++) {
                  unsigned paramIndex = dataIdx*16 + bitIdx;
                  unsigned bitIsSet = parameters & (1 << bitIdx) ? 1 : 0;
                  if (bitIsSet && (paramIndex < 128)) {
                    status = indexerPrepareParamRead(indexOffset, paramIndex);
                    if (!status) {
                      if (paramIndex < 30) {
                        unsigned iValue = -1;
                        status = getPlcMemoryUint(indexOffset+regSize,
                                                       &iValue, 2*regSize);
                        asynPrint(pasynUserController_, ASYN_TRACE_INFO,
                                  "%sparameters(%d)  paramIdx=%u iValue=%u status=%s (%d)\n",
                                  modNamEMC, axisNo, paramIndex, iValue,
                                  pasynManager->strStatus(status), (int)status);
                        if (!status) {
                          switch(paramIndex) {
                          case PARAM_IDX_MICROSTEPS_UINT32:
                            break;
                          case PARAM_IDX_OPMODE_AUTO_UINT32:
                            /* CNEN for EPICS */
                            pAxis->setIntegerParam(motorStatusGainSupport_, 1);
#ifdef POWERAUTOONOFFMODE2
                            pAxis->setIntegerParam(motorPowerAutoOnOff_, POWERAUTOONOFFMODE2);
                            pAxis->setDoubleParam(motorPowerOnDelay_,   6.0);
                            pAxis->setDoubleParam(motorPowerOffDelay_, -1.0);
#endif
                            break;
                          }
                        }
                      } else {
                        double fValue = -0.0;
                        status = getPlcMemoryDouble(indexOffset+regSize,
                                                    &fValue, 2*regSize);
                        asynPrint(pasynUserController_, ASYN_TRACE_INFO,
                                  "%sparameters(%d) paramIdx=%u fValue=%f status=%s (%d)\n",
                                  modNamEMC, axisNo, paramIndex, fValue,
                                  pasynManager->strStatus(status), (int)status);
                        if (!status) {
                          switch(paramIndex) {
                          case PARAM_IDX_ABS_MIN_FLOAT32:
                            setIntegerParam(axisNo, EthercatMCCfgDLLM_En_, 1);
                            pAxis->setDoubleParam(EthercatMCCfgDLLM_, fValue);
#ifdef motorLowLimitROString
                            setDoubleParam(motorLowLimitRO_, fValue);
#endif
                            break;
                          case PARAM_IDX_ABS_MAX_FLOAT32:
                            setIntegerParam(axisNo, EthercatMCCfgDHLM_En_, 1);
                            pAxis->setDoubleParam(EthercatMCCfgDHLM_, fValue);
#ifdef motorHighLimitROString
                            setDoubleParam(motorHighLimitRO_, fValue);
#endif
                            break;
                          case PARAM_IDX_USR_MIN_FLOAT32:
                            break;
                          case PARAM_IDX_USR_MAX_FLOAT32:
                            break;
                          case PARAM_IDX_WRN_MIN_FLOAT32:
                            break;
                          case PARAM_IDX_WRN_MAX_FLOAT32:
                            break;
                          case PARAM_IDX_FOLLOWING_ERR_WIN_FLOAT32:
                            pAxis->setDoubleParam(EthercatMCCfgPOSLAG_RB_, fValue);
                            pAxis->setDoubleParam(EthercatMCCfgPOSLAG_Tim_RB_, 0);
                            setIntegerParam(axisNo, EthercatMCCfgPOSLAG_En_RB_, 1);
                            break;
                          case PARAM_IDX_HYTERESIS_FLOAT32:
                            pAxis->setDoubleParam(EthercatMCCfgRDBD_RB_, fValue);
                            pAxis->setDoubleParam(EthercatMCCfgRDBD_Tim_RB_, 0);
                            setIntegerParam(axisNo, EthercatMCCfgRDBD_En_RB_, 1);
#ifdef motorRDBDROString
                            pAxis->setDoubleParam(motorRDBDRO_, fValue);
#endif
                            break;
                          case PARAM_IDX_REFSPEED_FLOAT32:
                            pAxis->setDoubleParam(EthercatMCVelToHom_, fValue);
                            break;
                          case PARAM_IDX_SPEED_FLOAT32:
                            pAxis->setDoubleParam(EthercatMCCfgVELO_, fValue);
#ifdef motorDefVelocityROString
                            pAxis->setDoubleParam(motorDefVelocityRO_, fValue);
#endif
                            break;
                          case PARAM_IDX_ACCEL_FLOAT32:
                            pAxis->setDoubleParam(EthercatMCCfgACCS_, fValue);

#ifdef motorDefJogAccROString
                            pAxis->setDoubleParam(motorDefJogAccRO_, fValue);
#endif
                            break;
                          case PARAM_IDX_IDLE_CURRENT_FLOAT32:
                            break;
                          case PARAM_IDX_MOVE_CURRENT_FLOAT32:
                            break;
                          case PARAM_IDX_MICROSTEPS_FLOAT32:
                            microsteps = fValue;
                            pAxis->setDoubleParam(EthercatMCCfgSREV_RB_, fullsrev * microsteps);
                            break;
                          case PARAM_IDX_STEPS_PER_UNIT_FLOAT32:
                            {
                              double urev = fabs(fullsrev / fValue);
                              pAxis->setDoubleParam(EthercatMCCfgUREV_RB_, urev);
                            }
                            break;
                          case PARAM_IDX_HOME_POSITION_FLOAT32:
                            pAxis->setDoubleParam(EthercatMCHomPos_, fValue);
                            break;
                          case PARAM_IDX_FUN_REFERENCE:
#ifdef  motorNotHomedProblemString
                            pAxis->setIntegerParam(motorNotHomedProblem_, MOTORNOTHOMEDPROBLEM_ERROR);
#endif
                            break;
                          }
                        }
                      }
                    } else {
                      asynPrint(pasynUserController_, ASYN_TRACE_INFO,
                                "%sparameters(%d) paramIdx=%u bitIdx=%u status=%s (%d)\n",
                                modNamEMC, axisNo, paramIndex, bitIdx,
                                pasynManager->strStatus(status), (int)status);
                    }
                  }
                }
              }
            break;
          default:
            ;
          }
        }
      }
    }
  }
  if (status) goto endPollIndexer;

#ifdef XXXEthercatMCrecordDescString
  setStringParam(EthercatMCrecordDesc_, desc);
#endif

#ifdef XXXXXEthercatMCrecordEguString
  setStringParam(EthercatMCrecordEgu_,
                 plcUnitTxtFromUnitCode(unitCode));
#endif

 endPollIndexer:
  /* Special case: asynDisabled means "no indexer found".
     That is OK, return asynSuccess */
  if (status == asynDisabled)
    return asynSuccess;
  return status;

  //getPlcMemoryBytes(0, &indexerMagic[0], sizeof(indexerMagic));
}