#!/usr/bin/env python
#
import epics
import unittest
import os
import sys
import time
from motor_lib import motor_lib
###



# Values to be used for backlash test
# Note: Make sure to use different values to hae a good
# test coverage
myVELO = 10.0   # positioning velocity
myACCL =  1.0   # Time to VELO, seconds
myAR   = myVELO / myACCL # acceleration, mm/sec^2

myJVEL = 5.0    # Jogging velocity
myJAR  = 6.0    # Jogging acceleration, mm/sec^2

myBVEL = 2.0    # backlash velocity
myBACC = 1.5    # backlash acceleration, seconds
myBAR  = myBVEL / myBACC  # backlash acceleration, mm/sec^2
myRTRY   = 3
myDLY    =  0.0
myBDST   = 0.0  # backlash destination, mm
myFRAC   = 1.0  #
myPOSlow = 48   #
myPOSmid = 72   # low + BDST
myPOShig = 96   # low + 2*BDST



motorRMOD_D = 0 # "Default"
motorRMOD_A = 1 # "Arithmetic"
motorRMOD_G = 2 # "Geometric"
motorRMOD_I = 3 # "In-Position"

#How we move: Absolute (without encoder) or relative (with encode via UEIP)
use_abs = 0
use_rel = 1
# Note: motorRMOD_I is always absolute !

noFRAC   = 1.0
withFRAC = 1.5

def setValueOnSimulator(self, motor, tc_no, var, value):
    var = str(var)
    value = str(value)
    outStr = 'Sim.this.' + var + '=' + value
    print '%s: DbgStrToMCU motor=%s var=%s value=%s outStr=%s' % \
          (tc_no, motor, var, value, outStr)
    assert(len(outStr) < 40)
    epics.caput(motor + '-DbgStrToMCU', outStr, wait=True)
    err = int(epics.caget(motor + '-Err', use_monitor=False))
    print '%s: DbgStrToMCU motor=%s var=%s value=%s err=%d' % \
          (tc_no, motor, var, value, err)
    assert (not err)


def motorInitAll(tself, motor, tc_no):
    setValueOnSimulator(tself, motor, tc_no, "nAmplifierPercent", 100)
    setValueOnSimulator(tself, motor, tc_no, "bAxisHomed",          1)
    setValueOnSimulator(tself, motor, tc_no, "fLowHardLimitPos",   15)
    setValueOnSimulator(tself, motor, tc_no, "fHighHardLimitPos", 165)
    setValueOnSimulator(tself, motor, tc_no, "setMRES_23", 0)
    setValueOnSimulator(tself, motor, tc_no, "setMRES_24", 0)

    # Prepare parameters for jogging and backlash
    epics.caput(motor + '.VELO', myVELO)
    epics.caput(motor + '.ACCL', myACCL)

    epics.caput(motor + '.JVEL', myJVEL)
    epics.caput(motor + '.JAR',  myJAR)

    epics.caput(motor + '.BVEL', myBVEL)
    epics.caput(motor + '.BACC', myBACC)
    epics.caput(motor + '.BDST', myBDST)
    epics.caput(motor + '.RTRY', myRTRY)
    epics.caput(motor + '.DLY',  myDLY)


def motorInitTC(tself, motor, tc_no, frac, rmod, encRel):
    epics.caput(motor + '.RMOD', rmod)
    epics.caput(motor + '.FRAC', frac)
    epics.caput(motor + '.UEIP', encRel)


def setMotorStartPos(tself, motor, tc_no, startpos):
    setValueOnSimulator(tself, motor, tc_no, "fActPosition", startpos)
    # Run a status update and a sync
    epics.caput(motor + '.STUP', 1)
    epics.caput(motor + '.SYNC', 1)


def writeExpFileRMOD_X(tself, motor, tc_no, frac, rmod, dbgFile, expFile, maxcnt, encRel, motorStartPos, motorEndPos):
    cnt = 0
    if motorEndPos - motorStartPos > 0:
        directionOfMove = 1
    else:
        directionOfMove = -1
    if myBDST > 0:
        directionOfBL = 1
    else:
        directionOfBL = -1

    singleMove = 0    
    if abs(motorEndPos - motorStartPos) <= abs(myBDST) and directionOfMove == directionOfBL:
        singleMove = 1

    if myBDST == 0.0:
        singleMove = 1

    print '%s: writeExpFileRMOD_X motor=%s encRel=%d frac=%f motorStartPos=%f motorEndPos=%f directionOfMove=%d directionOfBL=%d' % \
          (tc_no, motor, encRel, frac, motorStartPos, motorEndPos, directionOfMove, directionOfBL)

    if dbgFile != None:
        dbgFile.write('#%s: writeExpFileRMOD_X motor=%s rmod=%d encRel=%d motorStartPos=%f motorEndPos=%f directionOfMove=%d directionOfBL=%d\n' % \
          (tc_no, motor, rmod, encRel, motorStartPos, motorEndPos, directionOfMove, directionOfBL))

    if rmod == motorRMOD_I:
        maxcnt = 1 # motorRMOD_I means effecttivly "no retry"
        encRel = 0

    if singleMove:
        if myBDST == 0.0:
            # No backlash at all.
            # Use a single move with VELO and AR
            bvel = myVELO
            bar = myAR
        else:
            bvel = myBVEL
            bar = myBAR

        while cnt < maxcnt:
            # calculate the delta to move
            # The calculated delta is the scaled, and used for both absolute and relative
            # movements
            delta = motorEndPos - motorStartPos
            if cnt == 0:
                delta = delta * frac
            if cnt > 1:
                if rmod == motorRMOD_A:
                    # From motorRecord.cc:
                    #factor = (pmr->rtry - pmr->rcnt + 1.0) / pmr->rtry;
                    factor = 1.0 * (myRTRY -  cnt + 1.0) / myRTRY
                    delta = delta * factor
                elif rmod == motorRMOD_G:
                    #factor = 1 / pow(2.0, (pmr->rcnt - 1));
                    rcnt_1 = cnt - 1
                    factor = 1.0
                    while rcnt_1 > 0:
                        factor = factor / 2.0
                        rcnt_1 -= 1
                    delta = delta * factor


            if encRel:
                line1 = "move relative delta=%g max_velocity=%g acceleration=%g motorPosNow=%g" % \
                        (delta, bvel, bar, motorStartPos)
            else:
                line1 = "move absolute position=%g max_velocity=%g acceleration=%g motorPosNow=%g" % \
                        (motorStartPos + delta, bvel, bar, motorStartPos)
            expFile.write('%s\n' % (line1))
            cnt += 1
    else:
        # As we don't move the motor (it is simulated, we both times start at motorStartPos
        while cnt < maxcnt:
            # calculate the delta to move
            # The calculated delta is the scaled, and used for both absolute and relative
            # movements
            delta = (motorEndPos - motorStartPos - myBDST) * frac
            if cnt > 1:
                if rmod == motorRMOD_A:
                    # From motorRecord.cc:
                    #factor = (pmr->rtry - pmr->rcnt + 1.0) / pmr->rtry;
                    factor = 1.0 * (myRTRY -  cnt + 1.0) / myRTRY
                    delta = delta * factor
                elif rmod == motorRMOD_G:
                    #factor = 1 / pow(2.0, (pmr->rcnt - 1));
                    rcnt_1 = cnt - 1
                    factor = 1.0
                    while rcnt_1 > 0:
                        factor = factor / 2.0
                        rcnt_1 -= 1
                    delta = delta * factor

            if encRel:
                line1 = "move relative delta=%g max_velocity=%g acceleration=%g motorPosNow=%g" % \
                        (delta, myVELO, myAR, motorStartPos)
                # Move forward with backlash parameters
                # Note: This should be myBDST, but since we don't move the motor AND
                # the record uses the readback value, use "motorEndPos - motorStartPos"
                delta = motorEndPos - motorStartPos
                line2 = "move relative delta=%g max_velocity=%g acceleration=%g motorPosNow=%g" % \
                        (delta, myBVEL, myBAR, motorStartPos)
            else:
                line1 = "move absolute position=%g max_velocity=%g acceleration=%g motorPosNow=%g" % \
                        (motorStartPos + delta, myVELO, myAR, motorStartPos)
                # Move forward with backlash parameters
                line2 = "move absolute position=%g max_velocity=%g acceleration=%g motorPosNow=%g" % \
                        (motorEndPos, myBVEL, myBAR, motorStartPos)

            expFile.write('%s\n%s\n' % (line1, line2))
            cnt += 1




def positionAndRetry(tself, motor, tc_no, frac, rmod, encRel, motorStartPos, motorEndPos):
    lib = motor_lib()
    ###########
    # expected and actual
    fileName = "/tmp/" + motor.replace(':', '-') + "-" + str(tc_no)
    expFileName = fileName + ".exp"
    actFileName = fileName + ".act"
    dbgFileName = fileName + ".dbg"

    motorInitTC(tself, motor, tc_no, frac, rmod, encRel)
    setMotorStartPos(tself, motor, tc_no, motorStartPos)
    setValueOnSimulator(tself, motor, tc_no, "bManualSimulatorMode", 1)
    time.sleep(2)
    setValueOnSimulator(tself, motor, tc_no, "log", actFileName)
    time.sleep(2)
    #
    epics.caput(motor + '.VAL', motorEndPos, wait=True)
    setValueOnSimulator(tself, motor, tc_no, "dbgCloseLogFile", "1")
    time.sleep(2)
    setValueOnSimulator(tself, motor, tc_no, "bManualSimulatorMode", 0)

    # Create a "expected" file
    expFile=open(expFileName, 'w')
    if dbgFileName != None:
        dbgFile=open(dbgFileName, 'w')
    else:
        dbgFile = None
    # Positioning
    # 2 different ways to move:
    # - Within the backlash distance and into the backlash direction:
    #   single move with back lash parameters
    # - against the backlash direction -or- bigger than the backlash distance:
    #   two moves, first with moving, second with backlash parameters

    cnt = 1 + int(epics.caget(motor + '.RTRY'))
    writeExpFileRMOD_X(tself, motor, tc_no, frac, rmod, dbgFile, expFile, cnt, encRel, motorStartPos, motorEndPos)

    expFile.close()
    if dbgFileName != None:
        dbgFile.close()
    setValueOnSimulator(tself, motor, tc_no, "dbgCloseLogFile", "1")

    lib.cmpUnlinkExpectedActualFile(dbgFileName, expFileName, actFileName)



class Test(unittest.TestCase):
    lib = motor_lib()
    motor = os.getenv("TESTEDMOTORAXIS")

    def test_TC_14400(self):
        motorInitAll(self, self.motor, 14400)

    # motorRMOD_D = 0 # "Default"
    # position forward, absolute
    def test_TC_14401(self):
        positionAndRetry(self, self.motor, 14401, noFRAC, motorRMOD_D, use_abs, myPOSlow, myPOShig)

    # position forward, relative
    def test_TC_14402(self):
        positionAndRetry(self, self.motor, 14402, noFRAC, motorRMOD_D, use_rel, myPOSlow, myPOShig)

    # position forward, absolute
    def test_TC_14403(self):
        positionAndRetry(self, self.motor, 14403, withFRAC, motorRMOD_D, use_abs, myPOSlow, myPOShig)

    # position forward, relative
    def test_TC_14404(self):
        positionAndRetry(self, self.motor, 14404, withFRAC, motorRMOD_D, use_rel, myPOSlow, myPOShig)

    ###############################################################################
    # motorRMOD_A
    # position forward, absolute
    def test_TC_14411(self):
        positionAndRetry(self, self.motor, 14411, noFRAC, motorRMOD_A, use_abs, myPOSlow, myPOShig)

    # position forward, relative
    def test_TC_14412(self):
        positionAndRetry(self, self.motor, 14412, noFRAC, motorRMOD_A, use_rel, myPOSlow, myPOShig)

    # position forward, absolute
    def test_TC_14413(self):
        positionAndRetry(self, self.motor, 14413, withFRAC, motorRMOD_A, use_abs, myPOSlow, myPOShig)

    # position forward, relative
    def test_TC_14414(self):
        positionAndRetry(self, self.motor, 14414, withFRAC, motorRMOD_A, use_rel, myPOSlow, myPOShig)


    ###############################################################################
    # motorRMOD_G
    # position forward, absolute
    def test_TC_14421(self):
        positionAndRetry(self, self.motor, 14421, noFRAC, motorRMOD_G, use_abs, myPOSlow, myPOShig)

    # position forward, relative
    def test_TC_14422(self):
        positionAndRetry(self, self.motor, 14422, noFRAC, motorRMOD_G, use_rel, myPOSlow, myPOShig)

    # position forward, absolute
    def test_TC_14423(self):
        positionAndRetry(self, self.motor, 14423, withFRAC, motorRMOD_G, use_abs, myPOSlow, myPOShig)

    # position forward, relative
    def test_TC_14424(self):
        positionAndRetry(self, self.motor, 14424, withFRAC, motorRMOD_G, use_rel, myPOSlow, myPOShig)


    ###############################################################################
    # motorRMOD_I
    # position forward, absolute
    def test_TC_14431(self):
        positionAndRetry(self, self.motor, 14431, noFRAC, motorRMOD_I, use_abs, myPOSlow, myPOShig)

    # position forward, relative
    def test_TC_14432(self):
        positionAndRetry(self, self.motor, 14432, noFRAC, motorRMOD_I, use_rel, myPOSlow, myPOShig)

    # position forward, absolute
    def test_TC_14433(self):
        positionAndRetry(self, self.motor, 14433, withFRAC, motorRMOD_I, use_abs, myPOSlow, myPOShig)

    # position forward, relative
    def test_TC_14434(self):
        positionAndRetry(self, self.motor, 14434, withFRAC, motorRMOD_I, use_rel, myPOSlow, myPOShig)

