# Author: Adrian Cinal

import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

from defs.BoapAcpMessages import BoapAcpNodeId
import queue
import threading
import time

class EBoapLogSeverityLevel:
    Debug = 0
    Info = 1
    Warning = 2
    Error = 3

class BoapLogEvent:
    def __init__(self, eventId, payload):
        self.eventId = eventId
        self.payload = payload

    IdLogEntry = 0
    IdNewCallback = 1
    IdEnableDebugPrints = 2

class BoapLog:
    severityTags = {
        EBoapLogSeverityLevel.Debug : 'DBG',
        EBoapLogSeverityLevel.Info : 'INF',
        EBoapLogSeverityLevel.Warning : 'WRN',
        EBoapLogSeverityLevel.Error : 'ERR'
    }

    def __init__(self, commitCallback):
        self.debugPrintsEnable = False
        self.commitCallback = commitCallback
        self.loggerQueue = queue.Queue()

        def LoggerThreadEntryPoint():
            self.Debug('Logger thread entered')
            while True:
                event = self.loggerQueue.get()
                if BoapLogEvent.IdLogEntry == event.eventId:
                    self.commitCallback(event.payload)
                elif BoapLogEvent.IdNewCallback == event.eventId:
                    self.commitCallback = event.payload
                elif BoapLogEvent.IdEnableDebugPrints == event.eventId:
                    self.debugPrintsEnable = event.payload
                else:
                    self.Error('Unknown event %d received in logger queue' % event.eventId)

        self.loggerThread = threading.Thread(target=LoggerThreadEntryPoint, name='Logger', daemon=True)
        self.loggerThread.start()

    def SendEvent(self, event):
        self.loggerQueue.put(event)

    def Teardown(self):
        self.SendEvent(BoapLogEvent(BoapLogEvent.IdShutdownSignal, None))
        self.loggerThread.join()

    def PrintRaw(self, message):
        self.SendEvent(BoapLogEvent(BoapLogEvent.IdLogEntry, message))

    def Print(self, severity, message):
        def GetTimeStamp():
            return '<' + time.ctime().split()[3] + '>'

        logEntry = '[0x%02X] ' % BoapAcpNodeId.BOAP_ACP_NODE_ID_PC + GetTimeStamp() + ' ' \
            + self.severityTags[severity] + ' (' + threading.current_thread().getName() + '): ' + message
        self.PrintRaw(logEntry)

    def Error(self, message):
        self.Print(EBoapLogSeverityLevel.Error, message)

    def Warning(self, message):
        self.Print(EBoapLogSeverityLevel.Warning, message)

    def Info(self, message):
        self.Print(EBoapLogSeverityLevel.Info, message)

    def Debug(self, message):
        if (self.debugPrintsEnable):
            self.Print(EBoapLogSeverityLevel.Debug, message)

    def EnableDebugPrints(self):
        self.SendEvent(BoapLogEvent(BoapLogEvent.IdEnableDebugPrints, True))

    def DisableDebugPrints(self):
        self.SendEvent(BoapLogEvent(BoapLogEvent.IdEnableDebugPrints, False))

    def SetNewCallback(self, callback):
        self.SendEvent(BoapLogEvent(BoapLogEvent.IdNewCallback, callback))
