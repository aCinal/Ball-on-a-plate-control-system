# Author: Adrian Cinal

import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

from acp.BoapAcp import BoapAcpNodeId
from defs.BoapAcpMessages import BoapAcpMsgId
import threading
import queue
import time

class BoapAcpKeepalive:

    PING_PERIOD = 10
    RESPONSE_TIMEOUT = 1

    def __init__(self, acp, logger):
        self.acp = acp
        self.log = logger

        # Create the message queue
        self.msgQueue = queue.Queue()

        def KeepaliveThreadEntryPoint():
            self.log.Debug('Keepalive thread entered')

            while True:
                time.sleep(self.PING_PERIOD)

                # Ping the plant
                PingAndWaitForResponse(BoapAcpNodeId.BOAP_ACP_NODE_ID_PLANT)
                # Ping the controller
                PingAndWaitForResponse(BoapAcpNodeId.BOAP_ACP_NODE_ID_CONTROLLER)

        def PingAndWaitForResponse(node):
            request = self.acp.MsgCreate(node, BoapAcpMsgId.BOAP_ACP_PING_REQ)
            self.acp.MsgSend(request)
            # Receive the msg from the plant
            try:
                response = self.msgQueue.get(timeout=self.RESPONSE_TIMEOUT)
                # Assert message from the correct receiver
                if response.GetSender() != node:
                    self.log.Warning('Sent ping request to 0x%02X. 0x%02X responded instead'
                        % (node, response.GetSender()))
            except queue.Empty:
                self.log.Warning('Node 0x%02X failed to respond to ping' % node)

        self.log.Debug('Starting the keepalive thread...')
        # Create the keepalive thread
        self.keepaliveThread = threading.Thread(target=KeepaliveThreadEntryPoint, name='KeepaliveThread', daemon=True)
        self.keepaliveThread.start()
