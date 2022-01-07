# Author: Adrian Cinal

import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

from defs.BoapAcpMessages import BoapAcpMsgId
import threading

class BoapAcpLocalRouting:
    def __init__(self, queue, msgIds):
        self.queue = queue
        self.msgIds = msgIds

class BoapAcpGateway:
    def __init__(self, acp, logger, routingTable):
        self.acp = acp
        self.log = logger
        self.routingTable = routingTable

        def GatewayThreadEntryPoint():
            self.log.Debug('Gateway thread entered')

            while True:
                # Block on receive
                message = self.acp.MsgReceive()

                if message:
                    if BoapAcpMsgId.BOAP_ACP_PING_REQ == message.GetId():
                        # Respond to ping requests
                        pingResponse = self.acp.MsgCreate(message.GetSender(), BoapAcpMsgId.BOAP_ACP_PING_RESP)
                        self.acp.MsgSend(pingResponse)
                    elif BoapAcpMsgId.BOAP_ACP_LOG_COMMIT == message.GetId():
                        # Print log messages locally
                        self.log.PrintRaw('[0x%02X] ' % message.GetSender() + message.GetPayload().Message)
                    else:
                        # Forward remaining traffic to the appropriate queue
                        HandleApplicationMessage(message)

        def HandleApplicationMessage(message):
            for route in self.routingTable:
                if message.GetId() in route.msgIds:
                    route.queue.put(message)
                    return
            self.log.Warning('Message ID 0x%02X not found in the routing table (sender: 0x%02X)'
                % (message.GetId(), message.GetSender()))

        self.log.Debug('Starting the gateway thread...')
        # Create the gateway thread
        self.gatewayThread = threading.Thread(target=GatewayThreadEntryPoint, name='GatewayThread', daemon=True)
        self.gatewayThread.start()
