"""Handler of time-plots of the ball position."""

from typing import List, Tuple

import pyqtgraph as pg


class TimeTrace(pg.GraphicsLayoutWidget):
    """Handler of time-plots of the ball position."""

    SETPOINT_COLOR = (0, 255, 0)
    POSITION_COLOR = (255, 0, 0)

    class TimeTraceImpl:
        """Ball position plotted against time."""

        def __init__(
            self,
            plot_item: pg.PlotItem,
            buffer_size: int,
            color: Tuple[int, int, int],
        ) -> None:
            """Initialize a time-plot."""
            self.buffer_size = buffer_size
            self.value_buffer: List[float] = []
            self.time_buffer: List[int] = []
            self.curve = plot_item.plot(pen=pg.mkPen(color=color))
            self.pointer = 0

        def update(self, sample_number: int, value: float) -> None:
            """Update the plot."""
            if len(self.value_buffer) < self.buffer_size:
                self.value_buffer.append(value)
                self.time_buffer.append(sample_number)
            else:
                # Shift the buffers one sample left
                self.value_buffer[:-1] = self.value_buffer[1:]
                self.value_buffer[-1] = value
                self.time_buffer[:-1] = self.time_buffer[1:]
                self.time_buffer[-1] = sample_number
                self.pointer += 1
                self.curve.setPos(self.pointer, 0)

            self.curve.setData(self.time_buffer, self.value_buffer)

    def __init__(self, buffer_size: int) -> None:
        """Initialize a time-plot handler."""
        super().__init__()
        # Add the plots
        self.time_trace_plot_x = self.addPlot()
        self.nextRow()
        self.time_trace_plot_y = self.addPlot()

        self.time_trace_plot_x.setClipToView(True)
        self.time_trace_plot_x.setDownsampling(mode="peak")
        self.time_trace_plot_y.setClipToView(True)
        self.time_trace_plot_y.setDownsampling(mode="peak")

        self.time_trace_plot_x.setLabel(axis="bottom", text="n")
        self.time_trace_plot_x.setLabel(axis="left", text="x", units="mm")
        self.time_trace_plot_y.setLabel(axis="bottom", text="n")
        self.time_trace_plot_y.setLabel(axis="left", text="y", units="mm")

        # Initialize the time trace buffers
        self.traces = {}
        self.traces["setpoint_x"] = self.TimeTraceImpl(
            plot_item=self.time_trace_plot_x,
            buffer_size=buffer_size,
            color=self.SETPOINT_COLOR,
        )
        self.traces["setpoint_y"] = self.TimeTraceImpl(
            plot_item=self.time_trace_plot_y,
            buffer_size=buffer_size,
            color=self.SETPOINT_COLOR,
        )
        self.traces["position_x"] = self.TimeTraceImpl(
            plot_item=self.time_trace_plot_x,
            buffer_size=buffer_size,
            color=self.POSITION_COLOR,
        )
        self.traces["position_y"] = self.TimeTraceImpl(
            plot_item=self.time_trace_plot_y,
            buffer_size=buffer_size,
            color=self.POSITION_COLOR,
        )

    def update(
        self,
        sample_number: int,
        setpoint_x: float,
        position_x: float,
        setpoint_y: float,
        position_y: float,
    ) -> None:
        """Update the plots."""
        self.traces["setpoint_x"].update(sample_number, setpoint_x)
        self.traces["setpoint_y"].update(sample_number, setpoint_y)
        self.traces["position_x"].update(sample_number, position_x)
        self.traces["position_y"].update(sample_number, position_y)
