# Author: Adrian Cinal

import sys
import os
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from acp.BoapAcp import BoapAcp
from acp.BoapAcpGateway import BoapAcpGateway, BoapAcpLocalRouting
from acp.BoapAcpKeepalive import BoapAcpKeepalive
from defs.BoapAcpMessages import BoapAcpMsgId
from gui.BoapGui import BoapGui
from utils.BoapEarlyParser import BoapEarlyParser
from utils.BoapLog import BoapLog
import datetime
import time

def main():
    # Parse command line arguments
    optParser = BoapEarlyParser()
    opts = optParser.Parse(sys.argv)

    # Validate the options, fill in missing values with defaults etc.
    HandleOptionsEarly(opts)

    # Initialize the logger
    logger = BoapLog(GetEarlyLoggerCallback(opts['-e'] if '-e' in opts else None))

    if '-d' in opts:
        logger.EnableDebugPrints()

    # Initialize the ACP stack
    boapAcp = BoapAcp(logger, opts['-p'], opts['-b'])

    # Start keepalive thread
    keepalive = BoapAcpKeepalive(boapAcp, logger)

    # Initialize the GUI
    gui = BoapGui(logger, boapAcp)

    # Start the gateway thread
    gateway = BoapAcpGateway(
        boapAcp,
        logger,
        [
            BoapAcpLocalRouting(keepalive.msgQueue, [ BoapAcpMsgId.BOAP_ACP_PING_RESP ]),
            BoapAcpLocalRouting(gui.GetTracePanelMessageQueue(), [ BoapAcpMsgId.BOAP_ACP_BALL_TRACE_IND ]),
            BoapAcpLocalRouting(gui.GetControlPanelConfiguratorMessageQueue(), [
                BoapAcpMsgId.BOAP_ACP_GET_PID_SETTINGS_RESP,
                BoapAcpMsgId.BOAP_ACP_SET_PID_SETTINGS_RESP,
                BoapAcpMsgId.BOAP_ACP_GET_SAMPLING_PERIOD_RESP,
                BoapAcpMsgId.BOAP_ACP_SET_SAMPLING_PERIOD_RESP,
                BoapAcpMsgId.BOAP_ACP_GET_FILTER_ORDER_RESP,
                BoapAcpMsgId.BOAP_ACP_SET_FILTER_ORDER_RESP
            ]),
            BoapAcpLocalRouting(gui.GetControlPanelTraceEnableMessageQueue(), [ BoapAcpMsgId.BOAP_ACP_BALL_TRACE_ENABLE ])
        ]
    )

    # Run the PyQt event loop
    return gui.Run()

def HandleOptionsEarly(opts):
    # If no args, assume help
    if 0 == len(opts):
        opts['-h'] = True

    # Check for help
    if '-h' in opts.keys():
        BoapEarlyParser().Help()
        sys.exit(0)

    # Assert required parameters provided
    if '-p' not in opts.keys():
        raise Exception('Router port (-p/--port) not specified')

    if '-b' in opts.keys():
        # Assert correct argument type
        opts['-b'] = int(opts['-b'])
    else:
        # Set default value
        opts['-b'] = 115200

def GetEarlyLoggerCallback(eFlagValue):
    if 'stdout' == eFlagValue:
        return print
    elif 'file' == eFlagValue:
        def GetNewFilename():
            datestring = str(datetime.date.today()).replace('-', '_')
            timestring = time.ctime().split()[3].replace(':', '_')
            return ('boap_' + datestring + '_' + timestring + '_startup.log')
        filename = GetNewFilename()
        def ToFileCallback(logEntry):
            with open(filename, 'at') as fd:
                print(logEntry, file=fd)
        return ToFileCallback
    else:
        def NoLogCallback(logEntry):
            pass
        return NoLogCallback

if '__main__' == __name__:
    sys.exit(main())
