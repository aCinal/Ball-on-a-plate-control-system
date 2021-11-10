# Author: Adrian Cinal

import pyqtgraph as pg

class BoapGuiSpaceTrace(pg.PlotWidget):
    POSITION_DOT_SIZE = 15
    POSITION_DOT_COLOR = (200, 200, 255)
    SETPOINT_DOT_SIZE = 5
    SETPOINT_DOT_COLOR = (0, 255, 200)

    class SpaceTraceImpl:
        def __init__(self, plot, historyLen, dotSize, dotColor):
            self.historyLen = historyLen
            self.history = []
            self.scatterPlotItem = pg.ScatterPlotItem(size = dotSize, brush=pg.mkBrush(color=dotColor))
            plot.addItem(self.scatterPlotItem)

        def Update(self, x, y):
            if len(self.history) < self.historyLen:
                self.history.append({ 'pos': [x, y], 'data': 1 })
            else:
                self.history[:-1] = self.history[1:]
                self.history[-1] = { 'pos': [x, y], 'data': 1 }
            self.scatterPlotItem.clear()
            self.scatterPlotItem.addPoints(self.history)

    def __init__(self, historyLen):
        super().__init__()
        self.positionTrace = self.SpaceTraceImpl(self, historyLen, self.POSITION_DOT_SIZE, self.POSITION_DOT_COLOR)
        self.setpointTrace = self.SpaceTraceImpl(self, historyLen, self.SETPOINT_DOT_SIZE, self.SETPOINT_DOT_COLOR)
        self.setLabel('left', 'y', 'mm')
        self.setLabel('bottom', 'x', 'mm')

        self.setXRange(-322 / 2, 322 / 2)
        self.setYRange(-247 / 2, 247 / 2)
        self.setMouseEnabled(False, False)

    def Update(self, xSetpoint, xPosition, ySetpoint, yPosition):
        self.setpointTrace.Update(xSetpoint, ySetpoint)
        self.positionTrace.Update(xPosition, yPosition)
