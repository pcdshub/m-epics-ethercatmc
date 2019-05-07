/* Implementation of the indexer
  https://forge.frm2.tum.de/public/doc/plc/master/singlehtml/index.html#devices
 */

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "hw_motor.h"
#include "indexer.h"
#include "logerr_info.h"

/* type codes and sizes */
#define TYPECODE_INDEXER               0
#define SIZE_INDEXER                  38
#define TYPECODE_PARAMDEVICE_5008 0x5008
#define SIZE_PARAMDEVICE_5008        0x8


/* Well known unit codes */
#define UNITCODE_NONE                    0
#define UNITCODE_MM                 0xfd04
#define UNITCODE_DEGREE             0x000C

/* 3 devices: the indexer +  2 motors */
#define  NUM_DEVICES       3

typedef enum {
  idxStatusCodeRESET    = 0,
  idxStatusCodeIDLE     = 1,
  idxStatusCodePOWEROFF = 2,
  idxStatusCodeWARN     = 3,
  idxStatusCodeERR4     = 4,
  idxStatusCodeSTART    = 5,
  idxStatusCodeBUSY     = 6,
  idxStatusCodeSTOP     = 7,
  idxStatusCodeERROR    = 8,
  idxStatusCodeERR9     = 9,
  idxStatusCodeERR10    = 10,
  idxStatusCodeERR11    = 11,
  idxStatusCodeERR12    = 12,
  idxStatusCodeERR13    = 13,
  idxStatusCodeERR14    = 14,
  idxStatusCodeERR15    = 15
} idxStatusCodeType;


/* Param interface
 The bit 15..13 are coded like this: */
#define PARAM_IF_CMD_MASKPARAM_IF_CMD_MASK         0xE000
#define PARAM_IF_CMD_MASKPARAM_IF_IDX_MASK         0x1FFF

#define PARAM_IF_CMD_INVALID                       0x0000
#define PARAM_IF_CMD_DOREAD                        0x2000
#define PARAM_IF_CMD_DOWRITE                       0x4000
#define PARAM_IF_CMD_BUSY                          0x6000
#define PARAM_IF_CMD_DONE                          0x8000
#define PARAM_IF_CMD_ERR_NO_IDX                    0xA000
#define PARAM_IF_CMD_READONLY                      0xC000
#define PARAM_IF_CMD_RETRY_LATER                   0xE000

/* Param index values */
#define PARAM_IDX_OPMODE_AUTO_UINT32            1
#define PARAM_IDX_MICROSTEPS_UINT32             2
#define PARAM_IDX_ABS_MIN_FLOAT32              30
#define PARAM_IDX_ABS_MAX_FLOAT32              31
#define PARAM_IDX_USR_MIN_FLOAT32              32
#define PARAM_IDX_USR_MAX_FLOAT32              33
#define PARAM_IDX_WRN_MIN_FLOAT32              34
#define PARAM_IDX_WRN_MAX_FLOAT32              35
#define PARAM_IDX_FOLLOWING_ERR_WIN_FLOAT32    55
#define PARAM_IDX_HYTERESIS_FLOAT32            56
#define PARAM_IDX_REFSPEED_FLOAT32             58
#define PARAM_IDX_VBAS_FLOAT32                 59
#define PARAM_IDX_SPEED_FLOAT32                60
#define PARAM_IDX_ACCEL_FLOAT32                61
#define PARAM_IDX_IDLE_CURRENT_FLOAT32         62
#define PARAM_IDX_MOVE_CURRENT_FLOAT32         64
#define PARAM_IDX_MICROSTEPS_FLOAT32           67
#define PARAM_IDX_STEPS_PER_UNIT_FLOAT32       68
#define PARAM_IDX_HOME_POSITION_FLOAT32        69
#define PARAM_IDX_FUN_REFERENCE               133
#define PARAM_IDX_FUN_MOVE_VELOCITY           142

/*  Which parameters are available */
#define PARAM_AVAIL_0_15_OPMODE_AUTO_UINT32            (1 << (1))
#define PARAM_AVAIL_0_15_MICROSTEPS_UINT32             (1 << (2))

#define PARAM_AVAIL_16_31_ABS_MIN_FLOAT32              (1 << (30-16))
#define PARAM_AVAIL_16_31_ABS_MAX_FLOAT32              (1 << (31-16))

#define PARAM_AVAIL_32_47_USR_MIN_FLOAT32              (1 << (32-32))
#define PARAM_AVAIL_32_47_USR_MAX_FLOAT32              (1 << (33-32))
#define PARAM_AVAIL_32_47_WRN_MIN_FLOAT32              (1 << (34-32))
#define PARAM_AVAIL_32_47_WRN_MAX_FLOAT32              (1 << (35-32))

#define PARAM_AVAIL_48_63_FOLLOWING_ERR_WIN_FLOAT32    (1 << (55-48))
#define PARAM_AVAIL_48_63_HYTERESIS_FLOAT32            (1 << (56-48))
#define PARAM_AVAIL_48_63_REFSPEED_FLOAT32             (1 << (58-48))
#define PARAM_AVAIL_48_63_VBAS_FLOAT32                 (1 << (59-48))
#define PARAM_AVAIL_48_63_SPEED_FLOAT32                (1 << (60-48))
#define PARAM_AVAIL_48_63_ACCEL_FLOAT32                (1 << (61-48))
#define PARAM_AVAIL_48_63_IDLE_CURRENT_FLOAT32         (1 << (62-48))

#define PARAM_AVAIL_64_79_MOVE_CURRENT_FLOAT32         (1 << (64-64))
#define PARAM_AVAIL_64_79_MICROSTEPS_FLOAT32           (1 << (67-64))
#define PARAM_AVAIL_64_79_STEPS_PER_UNIT_FLOAT32       (1 << (68-64))
#define PARAM_AVAIL_64_79_HOME_POSITION_FLOAT32        (1 << (69-64))

#define PARAM_AVAIL_128_143_FUN_REFERENCE              (1 << (132-128))
#define PARAM_AVAIL_128_143_FUN_MOVE_VELOCITY          (1 << (142-128))



/* In the memory bytes, the indexer starts at 64 */
static unsigned offsetIndexer;

/* Info types of the indexer */
typedef struct {
    uint16_t   typeCode;
    uint16_t   size;
    uint16_t   offset;
    uint16_t   unit;
    uint16_t   flagsLow;
    uint16_t   flagsHigh;
    float      absMin;
    float      absMax;
} indexerInfoType0_type;

typedef struct {
    uint16_t   size;
} indexerInfoType1_type;

typedef struct {
  char name[33]; /* leave one byte for trailing '\0' */
} indexerInfoType4_type;

typedef struct {
  uint16_t  parameters[16]; /* counting 0..15 */
} indexerInfoType15_type;

typedef struct {
  uint16_t  typeCode;
  uint16_t  size;
  uint16_t  unitCode;
  uint16_t  paramAvail[16]; /* counting 0..15 */
  char      devName[34];
  char      auxName[8][34];
  float     absMin;
  float     absMax;
} indexerDeviceAbsStraction_type;


/* The paramDevice structure.
   floating point values are 4 bytes long,
   the whole structure uses 16 bytes, 8 words */
typedef struct {
  float     actualValue;
  float     targetValue;
  uint16_t  statusReasonAux;
  uint16_t  paramCtrl;
  float     paramValue;
} indexerDevice5008interface_type;


indexerDeviceAbsStraction_type indexerDeviceAbsStraction[NUM_DEVICES] =
{
  /* device 0, the indexer itself */
  { TYPECODE_INDEXER, SIZE_INDEXER,
    UNITCODE_NONE,
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    "indexer",
    { "", "", "", "", "", "", "", "" },
    0.0, 0.0
  },
  { TYPECODE_PARAMDEVICE_5008, SIZE_PARAMDEVICE_5008,
    UNITCODE_MM,
    {0,
     0,
     0,
     PARAM_AVAIL_48_63_SPEED_FLOAT32,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0},
    "SimAxis1",
    { "", "", "", "", "", "homing", "@home", "homed" },
    5.0, 175.0
  },
  { TYPECODE_PARAMDEVICE_5008, SIZE_PARAMDEVICE_5008,
    UNITCODE_DEGREE,
    {0, //PARAM_AVAIL_0_15_OPMODE_AUTO_UINT32,
     0,
     0,
     PARAM_AVAIL_48_63_SPEED_FLOAT32 | PARAM_AVAIL_48_63_ACCEL_FLOAT32,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0},
    "RotAxis2",
    { "", "", "", "", "", "homing", "@home", "homed" },
    -180.0, +180.0
  }
};


typedef struct
{
  double fHysteresis;
  double fVelocity;
  double fAcceleration;
} cmd_Motor_cmd_type;


static union {
  uint8_t  memoryBytes[1024];
  uint16_t memoryWords[512];
  struct {
    float    magic;
    uint16_t offset;
    uint16_t indexer_ack;
    /* Area for the indexer. union of the different info types */
    union {
      indexerInfoType0_type  infoType0;
      indexerInfoType1_type  infoType1;
      indexerInfoType4_type  infoType4;
      indexerInfoType15_type infoType15;
      } indexer;
    /* Remember that motor[0] is defined, but never used */
    indexerDevice5008interface_type motors[MAX_AXES];
    } memoryStruct;
} idxData;

static int initDone = 0;

/* values commanded to the motor */
static cmd_Motor_cmd_type cmd_Motor_cmd[MAX_AXES];


static void init(void)
{
  if (initDone) return;
  memset (&idxData, 0, sizeof(idxData));
  idxData.memoryStruct.magic = 2015.02;
  idxData.memoryStruct.offset = offsetIndexer;
  offsetIndexer =
    (unsigned)((void*)&idxData.memoryStruct.indexer_ack - (void*)&idxData);
  idxData.memoryStruct.offset = offsetIndexer;

  LOGINFO3("%s/%s:%d offsetIndexer=%u\n",
           __FILE__, __FUNCTION__, __LINE__,
           offsetIndexer);
  initDone = 1;
}

static void init_axis(int axis_no)
{
  static char init_done[MAX_AXES];
  const double MRES = 1;
  const double UREV = 60.0; /* mm/revolution */
  const double SREV = 2000.0; /* ticks/revolution */
  const double ERES = UREV / SREV;

  double ReverseMRES = (double)1.0/MRES;

  if (axis_no >= MAX_AXES || axis_no < 0) {
    return;
  }
  if (!init_done[axis_no]) {
    struct motor_init_values motor_init_values;
    double valueLow = -1.0 * ReverseMRES;
    double valueHigh = 186.0 * ReverseMRES;
    memset(&motor_init_values, 0, sizeof(motor_init_values));
    motor_init_values.ReverseERES = MRES/ERES;
    motor_init_values.ParkingPos = (100 + axis_no/10.0);
    motor_init_values.MaxHomeVelocityAbs = 5 * ReverseMRES;
    motor_init_values.lowHardLimitPos = valueLow;
    motor_init_values.highHardLimitPos = valueHigh;
    motor_init_values.hWlowPos = valueLow;
    motor_init_values.hWhighPos = valueHigh;

    hw_motor_init(axis_no,
                  &motor_init_values,
                  sizeof(motor_init_values));

    //cmd_Motor_cmd[axis_no].maximumVelocity = 50;
    //
    //cmd_Motor_cmd[axis_no].homeVeloTowardsHomeSensor = 10;
    //cmd_Motor_cmd[axis_no].homeVeloFromHomeSensor = 5;
    //cmd_Motor_cmd[axis_no].fPosition = getMotorPos(axis_no);
    //cmd_Motor_cmd[axis_no].referenceVelocity = 600;
    //cmd_Motor_cmd[axis_no].inTargetPositionMonitorWindow = 0.1;
    //cmd_Motor_cmd[axis_no].inTargetPositionMonitorTime = 0.02;
    //cmd_Motor_cmd[axis_no].inTargetPositionMonitorEnabled = 1;
    setMRES_23(axis_no, UREV);
    setMRES_24(axis_no, SREV);
    cmd_Motor_cmd[axis_no].fHysteresis = 0.1;
    cmd_Motor_cmd[axis_no].fVelocity = 1;
    cmd_Motor_cmd[axis_no].fAcceleration = 1;

    setAmplifierPercent(axis_no, 1);
    init_done[axis_no] = 1;
  }
}

static void
indexerMotorStatusRead(unsigned motor_axis_no,
                       indexerDevice5008interface_type *pIndexerDevice5008interface)
{
  unsigned ret = 0;
  unsigned statusReasonAux;
  idxStatusCodeType idxStatusCode;
  /* The following only works on little endian (?)*/
  statusReasonAux = pIndexerDevice5008interface->statusReasonAux;
  idxStatusCode = (idxStatusCodeType)(statusReasonAux >> 12);
  /* The following would be run in an own task in a PLC program.
     For the simulator, we hook the code into the read request
     RESET, START and STOP are commands from IOC.
     RESET is even the "wakeup" state.
  */

  switch (idxStatusCode) {
  case idxStatusCodeRESET:
    init_axis((int)motor_axis_no);
    motorStop(motor_axis_no);
    set_nErrorId(motor_axis_no, 0);
    break;
  case idxStatusCodeSTART:
    movePosition(motor_axis_no,
                 pIndexerDevice5008interface->targetValue,
                 0, /* int relative, */
                 cmd_Motor_cmd[motor_axis_no].fVelocity,
                 cmd_Motor_cmd[motor_axis_no].fAcceleration);
    break;
  case idxStatusCodeSTOP:
    motorStop(motor_axis_no);
    break;
  default:
    ;
  }
  statusReasonAux = 0;

  /* reason bits */
  if (getPosLimitSwitch(motor_axis_no))
    statusReasonAux |= 0x0800;
  if (getNegLimitSwitch(motor_axis_no))
    statusReasonAux |= 0x0400;
  {
    unsigned auxBitIdx = 0;
    for (auxBitIdx = 0; auxBitIdx < 7; auxBitIdx++) {
      if (!strcmp("homing",
                  (const char*)&indexerDeviceAbsStraction[motor_axis_no].auxName[auxBitIdx])) {
        if (isMotorHoming(motor_axis_no)) {
          statusReasonAux |= 1 << auxBitIdx;
        }
      }
    }
  }

  /* the status bits */
  if (get_bError(motor_axis_no))
    idxStatusCode = idxStatusCodeERROR;
  else if (!getAmplifierOn(motor_axis_no))
    idxStatusCode = idxStatusCodePOWEROFF;
  else if (isMotorMoving(motor_axis_no))
    idxStatusCode = idxStatusCodeBUSY;
  else if(statusReasonAux)
    idxStatusCode = idxStatusCodeWARN;
  else
    idxStatusCode = idxStatusCodeIDLE;

  ret = statusReasonAux | (idxStatusCode << 12);;
  pIndexerDevice5008interface->statusReasonAux = ret;
}


/* Reads a parameter.
   All return values are returned as double,
   the call will convert into int32 or real32 if needed
*/
static unsigned
indexerMotorParamRead(unsigned motor_axis_no,
                      unsigned paramIndex,
                      double *fRet)

{
  uint16_t ret = PARAM_IF_CMD_DONE | paramIndex;
  if (motor_axis_no >= MAX_AXES) {
    return PARAM_IF_CMD_ERR_NO_IDX;
  }

  init_axis((int)motor_axis_no);

  switch(paramIndex) {
  case PARAM_IDX_HYTERESIS_FLOAT32:
    *fRet = cmd_Motor_cmd[motor_axis_no].fHysteresis;
    return ret;
  case PARAM_IDX_SPEED_FLOAT32:
    *fRet = cmd_Motor_cmd[motor_axis_no].fVelocity;
    return ret;
  case PARAM_IDX_ACCEL_FLOAT32:
    *fRet = cmd_Motor_cmd[motor_axis_no].fAcceleration;
    return ret;
  default:
    break;
  }

  return PARAM_IF_CMD_ERR_NO_IDX | paramIndex;
}

/* Writes a parameter.
   the call will convert into int32 or real32 if needed
*/
static unsigned
indexerMotorParamWrite(unsigned motor_axis_no,
                       unsigned paramIndex,
                       double fValue)

{
  uint16_t ret = PARAM_IF_CMD_DONE | paramIndex;
  if (motor_axis_no >= MAX_AXES) {
    return PARAM_IF_CMD_ERR_NO_IDX;
  }

  init_axis((int)motor_axis_no);
  LOGINFO3("%s/%s:%d motor_axis_no=%u paramIndex=%u ,fValue=%f\n",
           __FILE__, __FUNCTION__, __LINE__,
           motor_axis_no, paramIndex, fValue);

  switch(paramIndex) {
  case PARAM_IDX_SPEED_FLOAT32:
    cmd_Motor_cmd[motor_axis_no].fVelocity = fValue;
    return ret;
  case PARAM_IDX_ACCEL_FLOAT32:
    cmd_Motor_cmd[motor_axis_no].fAcceleration = fValue;
    return ret;
    break;
  default:
    break;
  }

  return PARAM_IF_CMD_ERR_NO_IDX | paramIndex;
}

static void
indexerMotorParamInterface5008(unsigned motor_axis_no, unsigned offset)
{
  unsigned uValue = idxData.memoryWords[offset / 2];
  unsigned paramCommand = uValue & PARAM_IF_CMD_MASKPARAM_IF_CMD_MASK;
  unsigned paramIndex = uValue & PARAM_IF_CMD_MASKPARAM_IF_IDX_MASK;
  uint16_t ret = (uint16_t)uValue;
  LOGINFO3("%s/%s:%d motor_axis_no=%u offset=%u uValue=0x%x\n",
           __FILE__, __FUNCTION__, __LINE__,
           motor_axis_no, offset, uValue);

  if (paramCommand == PARAM_IF_CMD_DOREAD) {
    double fRet;
    /* do the read */
    ret = indexerMotorParamRead(motor_axis_no,
                                paramIndex,
                                &fRet);
    /* put DONE (or ERROR) into the process image */
    idxData.memoryWords[offset / 2] = ret;
    if ((ret & PARAM_IF_CMD_MASKPARAM_IF_CMD_MASK) == PARAM_IF_CMD_DONE) {
      float fFloat = (float)fRet;
      switch(paramIndex) {
      case PARAM_IDX_HYTERESIS_FLOAT32:
      case PARAM_IDX_SPEED_FLOAT32:
      case PARAM_IDX_ACCEL_FLOAT32:
        memcpy(&idxData.memoryWords[(offset/2) + 1],
               &fFloat, 4);
        break;
      }
    }
  } else if (paramCommand == PARAM_IF_CMD_DOWRITE) {
    float fFloat;
    ret = PARAM_IF_CMD_ERR_NO_IDX;

    memcpy(&fFloat, &idxData.memoryWords[(offset/2) + 1], 4);
    switch(paramIndex) {
    case PARAM_IDX_SPEED_FLOAT32:
    case PARAM_IDX_ACCEL_FLOAT32:
      ret = indexerMotorParamWrite(motor_axis_no,
                                   paramIndex,
                                   (double)fFloat);
      break;
    case PARAM_IDX_FUN_REFERENCE:
      {
        int direction = 0;
        double max_velocity = 2;
        double acceleration = 3;
        moveHome(motor_axis_no,
                 direction,
                 max_velocity,
                 acceleration);
        ret = PARAM_IF_CMD_DONE | paramIndex;
      }
    case PARAM_IDX_FUN_MOVE_VELOCITY:
      {
        int direction = fFloat > 0.0;
        moveVelocity(motor_axis_no,
                     direction,
                     fabs(fFloat),
                     cmd_Motor_cmd[motor_axis_no].fAcceleration);
        ret = PARAM_IF_CMD_DONE | paramIndex;
      }
      break;
    }
    /* put DONE (or ERROR) into the process image */
    idxData.memoryWords[offset / 2] = ret;
  }
  LOGINFO6("%s/%s:%d indexerMotorParamRead motor_axis_no=%u paramIndex=%u uValue=%x ret=%x\n",
           __FILE__, __FUNCTION__, __LINE__,
           motor_axis_no, paramIndex, uValue, ret);

}

static int indexerHandleIndexerCmd(unsigned offset,
                                   unsigned len_in_PLC,
                                   unsigned uValue)
{
  unsigned devNum = uValue & 0xFF;
  unsigned infoType = (uValue >> 8) & 0x7F;
  unsigned maxDevNum = NUM_DEVICES - 1;
  LOGINFO3("%s/%s:%d offset=%u len_in_PLC=%u uValue=0x%x devNum=%u maxDevNum=%u infoType=%u\n",
           __FILE__, __FUNCTION__, __LINE__,
           offset, len_in_PLC,
           uValue, devNum, maxDevNum, infoType);
  memset(&idxData.memoryStruct.indexer, 0, sizeof(idxData.memoryStruct.indexer));
  idxData.memoryStruct.indexer_ack = uValue;
  if (devNum >= NUM_DEVICES) {
    idxData.memoryStruct.indexer_ack |= 0x8000; /* ACK */
    return 0;
  }
  switch (infoType) {
    case 0:
      /* get values from device table */
      idxData.memoryStruct.indexer.infoType0.typeCode = indexerDeviceAbsStraction[devNum].typeCode;
      idxData.memoryStruct.indexer.infoType0.size = indexerDeviceAbsStraction[devNum].size;
      idxData.memoryStruct.indexer.infoType0.unit = indexerDeviceAbsStraction[devNum].unitCode;
      /* TODO: calc the flags from lenght of AUX strings */
      //idxData.memoryStruct.indexer.infoType0.flagsLow = indexerDeviceAbsStraction[devNum].flagsLow;
      //idxData.memoryStruct.indexer.infoType0.flagsHigh = indexerDeviceAbsStraction[devNum].flagsHigh;
      idxData.memoryStruct.indexer.infoType0.absMin = indexerDeviceAbsStraction[devNum].absMin;
      idxData.memoryStruct.indexer.infoType0.absMax = indexerDeviceAbsStraction[devNum].absMax;
      if (!devNum) {
        /* The indexer himself. */
        idxData.memoryStruct.indexer.infoType0.offset = offsetIndexer;
        idxData.memoryStruct.indexer.infoType0.flagsHigh = 0x8000; /* extended indexer */
      } else {
        unsigned auxIdx;
        unsigned flagsLow = 0;
        unsigned maxAuxIdx;
        unsigned offset;
        maxAuxIdx = sizeof(indexerDeviceAbsStraction[devNum].auxName) /
          sizeof(indexerDeviceAbsStraction[devNum].auxName[0]);

        for (auxIdx = maxAuxIdx; auxIdx; auxIdx--) {
          if (strlen(indexerDeviceAbsStraction[devNum].auxName[auxIdx])) {
            flagsLow |= 1;
          }
          flagsLow = flagsLow << 1;
          LOGINFO3("%s/%s:%d auxIdx=%u flagsLow=0x%x\n",
                   __FILE__, __FUNCTION__, __LINE__,
                   auxIdx, flagsLow);
        }
        idxData.memoryStruct.indexer.infoType0.flagsLow = flagsLow;
        /* Offset to the first motor */
        offset = (unsigned)((void*)&idxData.memoryStruct.motors[devNum] - (void*)&idxData);
        /* TODO: Support other interface types */

        idxData.memoryStruct.indexer.infoType0.offset = offset;
      }
      LOGINFO3("%s/%s:%d idxData=%p indexer=%p delta=%u typeCode=%x size=%u offset=%u flagsLow=0x%x ack=0x%x\n",
               __FILE__, __FUNCTION__, __LINE__,
               &idxData, &idxData.memoryStruct.indexer,
               (unsigned)((void*)&idxData.memoryStruct.indexer - (void*)&idxData),
               idxData.memoryStruct.indexer.infoType0.typeCode,
               idxData.memoryStruct.indexer.infoType0.size,
               idxData.memoryStruct.indexer.infoType0.offset,
               idxData.memoryStruct.indexer.infoType0.flagsLow,
               idxData.memoryStruct.indexer_ack);

      idxData.memoryStruct.indexer_ack |= 0x8000; /* ACK */
      return 0;
    case 1:
      idxData.memoryStruct.indexer.infoType1.size =
        indexerDeviceAbsStraction[devNum].size;
      idxData.memoryStruct.indexer_ack |= 0x8000; /* ACK */
      return 0;
    case 4:
      /* get values from device table */
      strncpy(&idxData.memoryStruct.indexer.infoType4.name[0],
              indexerDeviceAbsStraction[devNum].devName,
              sizeof(idxData.memoryStruct.indexer.infoType4.name));
      LOGINFO3("%s/%s:%d devName=%s idxName=%s\n",
               __FILE__, __FUNCTION__, __LINE__,
               indexerDeviceAbsStraction[devNum].devName,
               &idxData.memoryStruct.indexer.infoType4.name[0]);
      idxData.memoryStruct.indexer_ack |= 0x8000; /* ACK */
      return 0;
    case 5: /* version */
    case 6: /* author 1 */
    case 7: /* author 2 */
      idxData.memoryStruct.indexer_ack |= 0x8000; /* ACK */
      return 0;
    case 15:
      memcpy(&idxData.memoryStruct.indexer.infoType15,
              indexerDeviceAbsStraction[devNum].paramAvail,
              sizeof(idxData.memoryStruct.indexer.infoType15));
      idxData.memoryStruct.indexer_ack |= 0x8000; /* ACK */
      return 0;
    default:
      if (infoType >= 16 && infoType <= 23) {
        /* Support for aux bits 7..0
           Bits 23..16 are not (yet) supported */
        strncpy(&idxData.memoryStruct.indexer.infoType4.name[0],
                indexerDeviceAbsStraction[devNum].auxName[infoType-16],
                sizeof(idxData.memoryStruct.indexer.infoType4.name));
        idxData.memoryStruct.indexer_ack |= 0x8000; /* ACK */
        return 0;
      }
      return __LINE__;
    }
  return __LINE__;
}

/*************************************************************************/
int indexerHandleADS_ADR_getUInt(unsigned adsport,
                                 unsigned offset,
                                 unsigned len_in_PLC,
                                 unsigned *uValue)
{
  unsigned ret;
  init();
  if (offset + len_in_PLC >= sizeof(idxData))
    return __LINE__;
  if (offset & 0x1) /* Must be even */
    return __LINE__;
  if (len_in_PLC == 2) {
    ret = idxData.memoryWords[offset / 2];
    *uValue = ret;
    /*
    LOGINFO3("%s/%s:%d adsport=%u offset=%u len_in_PLC=%u mot1=%u ret=%u (0x%x)\n",
             __FILE__, __FUNCTION__, __LINE__,
             adsport,
             offset,
             len_in_PLC,
             offsetMotor1StatusReasonAux,
             ret, ret);
    */
    return 0;
  } else if (len_in_PLC == 4) {
    ret = idxData.memoryWords[offset / 2] +
      (idxData.memoryWords[(offset / 2) + 1] <<16);
    *uValue = ret;
    return 0;
  }
  return __LINE__;
}

int indexerHandleADS_ADR_putUInt(unsigned adsport,
                                 unsigned offset,
                                 unsigned len_in_PLC,
                                 unsigned uValue)
{
  init();
  LOGINFO6("%s/%s:%d adsport=%u offset=%u len_in_PLC=%u uValue=%u (%x)\n",
           __FILE__, __FUNCTION__, __LINE__,
           adsport,
           offset,
           len_in_PLC,
           uValue, uValue);
  if (offset == offsetIndexer) {
    return indexerHandleIndexerCmd(offset, len_in_PLC, uValue);
  } else if (offset < (sizeof(idxData) / sizeof(uint16_t))) {
    idxData.memoryWords[offset / 2] = uValue;
    return 0;
  }
  LOGERR("%s/%s:%d adsport=%u offset=%u len_in_PLC=%u uValue=%u (%x)sizeof=%lu\n",
         __FILE__, __FUNCTION__, __LINE__,
         adsport, offset, len_in_PLC, uValue, uValue,
         (unsigned long)(sizeof(idxData) / sizeof(uint16_t)));

  return __LINE__;
}

int indexerHandleADS_ADR_getFloat(unsigned adsport,
                                  unsigned offset,
                                  unsigned len_in_PLC,
                                  double *fValue)
{
  float fRet;
  init();
  if (offset + len_in_PLC >= sizeof(idxData))
    return 1;
  if (len_in_PLC == 4) {
    memcpy(&fRet,
           &idxData.memoryBytes[offset],
           sizeof(fRet));
    *fValue = (double)fRet;
    return 0;
  }
  return __LINE__;
}


int indexerHandleADS_ADR_putFloat(unsigned adsport,
                                  unsigned offset,
                                  unsigned len_in_PLC,
                                  double fValue)
{
  init();
  LOGINFO3("%s/%s:%d adsport=%u offset=%u len_in_PLC=%u fValue=%f\n",
           __FILE__, __FUNCTION__, __LINE__,
           adsport,
           offset,
           len_in_PLC,
           fValue);
  if (offset + len_in_PLC >= sizeof(idxData))
    return 1;
  if (len_in_PLC == 4) {
    float fFloat = (float)fValue;
    memcpy(&idxData.memoryBytes[offset],
           &fFloat,
           len_in_PLC);
    return 0;
  }
  return __LINE__;
};


int indexerHandleADS_ADR_getString(unsigned adsport,
                                   unsigned offset,
                                   unsigned len_in_PLC,
                                   char **sValue)
{
  init();
  if (offset + len_in_PLC > sizeof(idxData)) {
    RETURN_ERROR_OR_DIE(__LINE__,
                        "%s/%s:%d out of range: offset=%u len_in_PLC=%u",
                       __FILE__, __FUNCTION__, __LINE__,
                       offset, len_in_PLC);
  }
  *sValue = (char *)&idxData.memoryBytes[offset];
  return 0;
};

int indexerHandleADS_ADR_getMemory(unsigned adsport,
                                   unsigned offset,
                                   unsigned len_in_PLC,
                                   void *buf)
{
  init();
  if (offset + len_in_PLC > sizeof(idxData)) {
    RETURN_ERROR_OR_DIE(__LINE__,
                        "%s/%s:%d out of range: offset=%u len_in_PLC=%u",
                        __FILE__, __FUNCTION__, __LINE__,
                        offset, len_in_PLC);
  }
  memcpy(buf, &idxData.memoryBytes[offset], len_in_PLC);
  return 0;
};

int indexerHandleADS_ADR_setMemory(unsigned adsport,
                                   unsigned offset,
                                   unsigned len_in_PLC,
                                   void *buf)
{
  init();
  if (offset + len_in_PLC > sizeof(idxData)) {
    RETURN_ERROR_OR_DIE(__LINE__,
                        "%s/%s:%d out of range: offset=%u len_in_PLC=%u",
                        __FILE__, __FUNCTION__, __LINE__,
                        offset, len_in_PLC);
  }
  memcpy(&idxData.memoryBytes[offset], buf, len_in_PLC);
  return 0;
};

void indexerHandlePLCcycle(void)
{
  unsigned devNum = 0;
  init();
  while (devNum < NUM_DEVICES) {
    LOGINFO3("%s/%s:%d devNum=%u typeCode=0x%x\n",
             __FILE__, __FUNCTION__, __LINE__,
             devNum, indexerDeviceAbsStraction[devNum].typeCode);

    switch (indexerDeviceAbsStraction[devNum].typeCode) {
    case TYPECODE_PARAMDEVICE_5008:
      {
        float fRet;
        unsigned offset;
        offset = (unsigned)((void*)&idxData.memoryStruct.motors[devNum].actualValue -
                            (void*)&idxData);

        fRet = getMotorPos((int)devNum);
        LOGINFO6("%s/%s:%d devNum=%u offset=%u fRet=%f\n",
                 __FILE__, __FUNCTION__, __LINE__,
                 devNum, offset, (double)fRet);
        memcpy(&idxData.memoryBytes[offset], &fRet, sizeof(fRet));
        /* status */
        indexerMotorStatusRead(devNum, &idxData.memoryStruct.motors[devNum]);

        /* param interface */
        offset = (unsigned)((void*)&idxData.memoryStruct.motors[devNum].paramCtrl -
                            (void*)&idxData);
        LOGINFO3("%s/%s:%d devNum=%u offset=%u\n",
                 __FILE__, __FUNCTION__, __LINE__,
                 devNum, offset);
        indexerMotorParamInterface5008(devNum, offset);
      }
      break;
    case TYPECODE_INDEXER:
      {
        uint16_t indexer_ack = idxData.memoryStruct.indexer_ack;
        LOGINFO6("%s/%s:%d devNum=%u indexer_ack=0x%x\n",
                 __FILE__, __FUNCTION__, __LINE__,
                 devNum, indexer_ack);

        if (!(indexer_ack & 0x8000)) {
          unsigned len_in_PLC = sizeof(indexer_ack);
          indexerHandleIndexerCmd(offsetIndexer, len_in_PLC, indexer_ack);
        }
      }
    default:
    break;
    }
    devNum++;
  }
}
