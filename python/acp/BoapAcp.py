# Author: Adrian Cinal

import sys
import os

sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

from defs.BoapAcpMessages import BoapAcpMsgGetPayloadById, BoapAcpNodeId, BoapAcpMsgId
from defs.BoapAcpErrors import BoapAcpMalformedMessageError
import serial
import threading
import struct

BOAP_ACP_WAIT_FOREVER = -1

class BoapAcp:
    HEADER_SIZE = 4
    PAYLOAD_RECV_TO = 1

    def __init__(self, logger, port, baud):
        self.log = logger
        self.port = port
        self.baud = baud
        self.writeLock = threading.Lock()

        # Open the serial port
        self.log.Info('Opening the serial port...')
        self.serial = serial.Serial(port = self.port, baudrate = self.baud)

    def MsgCreate(self, receiver, msgId):
        return self.Msg(msgId, BoapAcpNodeId.BOAP_ACP_NODE_ID_PC, receiver)

    def MsgSend(self, msg):
        serialMessage = bytearray([msg.msgId, msg.sender, msg.receiver, msg.payloadSize])
        if msg.payload:
            serialMessage += bytearray(msg.payload.Serialize())
        # Call serial API
        with self.writeLock:
            self.serial.write(serialMessage)

    def MsgReceive(self):
        # Block on read (no timeout)
        msg = None
        while not msg:
            # Read from the serial port
            header = self.serial.read(self.HEADER_SIZE)

            # Validate the header
            if not self.ValidHeader(header):
                # Flush the OS buffer...
                self.serial.read_all()
                # ...and try again
                continue

            # Parse the header
            msgId, sender, receiver, payloadSize = header

            payload = None
            if payloadSize:
                # Received the payload if any declared in the header
                timedOut = False
                def timerCallback():
                    nonlocal timedOut
                    timedOut = True
                    self.serial.cancel_read()
                # Start a timer to not block waiting for payload indefinitely
                timer = threading.Timer(self.PAYLOAD_RECV_TO, timerCallback)
                timer.start()
                payload = self.serial.read(payloadSize)
                timer.cancel()
                if timedOut:
                    self.log.Warning('Failed to receive the payload in time (header: msgId=0x%02X, sender=0x%02X, receiver=0x%02X, payloadSize=%d)' \
                        % (msgId, sender, receiver, payloadSize))
                    continue

            try:
                msg = self.Msg(msgId, sender, receiver, payload)
            except BoapAcpMalformedMessageError:
                self.log.Error('Failed to parse the payload of message 0x%02X from 0x%02X' % (msgId, sender))
                # Flush the OS buffer
                self.serial.read_all()
        return msg

    def ValidHeader(self, header):
        # Parse the header
        msgId, sender, receiver, _ = header
        return receiver == BoapAcpNodeId.BOAP_ACP_NODE_ID_PC and sender in list(BoapAcpNodeId) and msgId in list(BoapAcpMsgId)

    class Msg:
        def __init__ (self, msgId, sender, receiver, serialPayload = None):
            self.msgId = msgId
            self.sender = sender
            self.receiver = receiver
            self.serialPayload = serialPayload
            self.payloadSize = 0
            self.payload = BoapAcpMsgGetPayloadById(self.msgId)
            if self.payload:
                if serialPayload:
                    try:
                        self.payload.Parse(serialPayload)
                    except struct.error:
                        raise BoapAcpMalformedMessageError
                self.payloadSize = self.payload.Size()

        def GetId(self):
            return self.msgId

        def GetSender(self):
            return self.sender

        def GetReceiver(self):
            return self.receiver

        def GetPayload(self):
            return self.payload

        def GetPayloadSize(self):
            return self.payloadSize
