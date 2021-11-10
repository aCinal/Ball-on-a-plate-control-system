# Author: Adrian Cinal

from pyqtgraph import PlotItem, GraphicsLayoutWidget
from numpy import empty

class BoapGuiTimeTrace(GraphicsLayoutWidget):
    class TimeTraceImpl:
        def __init__(self, plotItem, bufferSize):
            self.bufferSize = bufferSize
            self.valueBuffer = []
            self.timeBuffer = []
            self.curve = plotItem.plot()
            self.pointer = 0

        def Update(self, sampleNumber, value):
            if len(self.valueBuffer) < self.bufferSize:
                self.valueBuffer.append(value)
                self.timeBuffer.append(sampleNumber)
            else:
                # Shift the buffers one sample left
                self.valueBuffer[:-1] = self.valueBuffer[1:]
                self.valueBuffer[-1] = value
                self.timeBuffer[:-1] = self.timeBuffer[1:]
                self.timeBuffer[-1] = sampleNumber
                self.pointer += 1
                self.curve.setPos(self.pointer, 0)

            self.curve.setData(self.timeBuffer, self.valueBuffer)

    def __init__(self, bufferSize):
        super().__init__()
        # Add the plots
        self.xTimeTracePlot = self.addPlot()
        self.nextRow()
        self.yTimeTracePlot = self.addPlot()

        self.xTimeTracePlot.setClipToView(True)
        self.xTimeTracePlot.setDownsampling(mode='peak')
        self.yTimeTracePlot.setClipToView(True)
        self.yTimeTracePlot.setDownsampling(mode='peak')

        self.xTimeTracePlot.setLabel('bottom', 'n')
        self.xTimeTracePlot.setLabel('left', 'x', 'mm')
        self.yTimeTracePlot.setLabel('bottom', 'n')
        self.yTimeTracePlot.setLabel('left', 'y', 'mm')

        # Initialize the time trace buffers
        self.traces = {}
        self.traces['xSetpoint'] = self.TimeTraceImpl(self.xTimeTracePlot, bufferSize)
        self.traces['ySetpoint'] = self.TimeTraceImpl(self.yTimeTracePlot, bufferSize)
        self.traces['xPosition'] = self.TimeTraceImpl(self.xTimeTracePlot, bufferSize)
        self.traces['yPosition'] = self.TimeTraceImpl(self.yTimeTracePlot, bufferSize)

    def Update(self, sampleNumber, xSetpoint, xPosition, ySetpoint, yPosition):
        self.traces['xSetpoint'].Update(sampleNumber, xSetpoint)
        self.traces['ySetpoint'].Update(sampleNumber, ySetpoint)
        self.traces['xPosition'].Update(sampleNumber, xPosition)
        self.traces['yPosition'].Update(sampleNumber, yPosition)
