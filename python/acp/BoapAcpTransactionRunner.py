# Author: Adrian Cinal

import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

from defs.BoapAcpErrors import BoapAcpTransactionError
import queue

class BoapAcpTransactionRunner:
    def __init__(self, acp, receiveTimeout, msgQueue):
        self.acp = acp
        self.receiveTimeout = receiveTimeout
        self.msgQueue = msgQueue

    def RunTransaction(self, request, expectedResponseId):
        # Send the request message
        self.acp.MsgSend(request)
        # Wait for the response
        try:
            response = self.msgQueue.get(timeout=self.receiveTimeout)
            if expectedResponseId != response.GetId():
                raise BoapAcpTransactionError('ACP transaction failed. Expected response 0x%02X, instead received 0x%02X' \
                    % (expectedResponseId, response.GetId()))
            return response.GetPayload()
        except queue.Empty:
            raise BoapAcpTransactionError('ACP transaction failed. No response received for message 0x%02X (expected response: 0x%02X)' \
                % (request.GetId(), expectedResponseId))
