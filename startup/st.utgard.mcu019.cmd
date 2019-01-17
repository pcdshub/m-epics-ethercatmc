# -----------------------------------------------------------------------------
# Douglas version
# -----------------------------------------------------------------------------
require asyn,4.33
require EthercatMC,2.1.0

# -----------------------------------------------------------------------------
# IOC common settings
# -----------------------------------------------------------------------------
epicsEnvSet("MOTOR_PORT",    "$(SM_MOTOR_PORT=MCU1)")
epicsEnvSet("IPADDR",        "$(SM_IPADDR=10.4.3.205)")
epicsEnvSet("IPPORT",        "$(SM_IPPORT=5000)")
epicsEnvSet("ASYN_PORT",     "$(SM_ASYN_PORT=MC_CPU1)")
epicsEnvSet("PREFIX",        "$(SM_PREFIX=LabS-ESSIIP:)")
epicsEnvSet("P",             "$(SM_PREFIX=LabS-ESSIIP:)")
epicsEnvSet("EGU",           "$(SM_EGU=mm)")
epicsEnvSet("PREC",          "$(SM_PREC=3)")

# -----------------------------------------------------------------------------
# EtherCAT MC Controller
# -----------------------------------------------------------------------------
< EthercatMCController.cmd


# -----------------------------------------------------------------------------
# Axis 1 configuration and instantiation
# -----------------------------------------------------------------------------
epicsEnvSet("MOTOR_NAME",    "$(SM_MOTOR_NAME=MC-MCU-019:m1)")
epicsEnvSet("AXIS_NO",       "$(SM_AXIS_NO=1)")
epicsEnvSet("DESC",          "$(SM_DESC=Lower=Right)")
epicsEnvSet("AXISCONFIG",    "HomProc=1;HomPos=-63;encoder=ADSPORT=501/.ADR.16#3040010,16#80000049,2,2")

< EthercatMCAxis.cmd
< EthercatMCAxisdebug.cmd
< EthercatMCAxishome.cmd

# -----------------------------------------------------------------------------
# Axis 2 configuration and instantiation
# -----------------------------------------------------------------------------
epicsEnvSet("MOTOR_NAME",    "$(SM_MOTOR_NAME=MC-MCU-019:m2)")
epicsEnvSet("AXIS_NO",       "$(SM_AXIS_NO=2)")
epicsEnvSet("DESC",          "$(SM_DESC=Upper=Left)")
epicsEnvSet("AXISCONFIG",    "HomProc=2;HomPos=64;encoder=ADSPORT=501/.ADR.16#3040010,16#8000004F,2,2")

< EthercatMCAxis.cmd
< EthercatMCAxisdebug.cmd
< EthercatMCAxishome.cmd

# -----------------------------------------------------------------------------
# # Logical axes and slit configuration and instantiation
# -----------------------------------------------------------------------------
epicsEnvSet("SLIT",          "$(SM_SLIT=MC-SLT-01:SltH-)")
epicsEnvSet("mXp",           "$(SM_mXp=MC-MCU-02:m2)")
epicsEnvSet("mXn",           "$(SM_mXp=MC-MCU-02:m1)")

< EthercatMC2slit.cmd
