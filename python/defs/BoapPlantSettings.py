# Author: Adrian Cinal

class BoapPlantSettings:
    def __init__(self):
        class BoapPlantAxisSettings:
            def __init__(self):
                self.ProportionalGain = 0.0
                self.IntegralGain = 0.0
                self.DerivativeGain = 0.0
                self.FilterOrder = 5
        self.SamplingPeriod = 0.05
        self.XAxis = BoapPlantAxisSettings()
        self.YAxis = BoapPlantAxisSettings()
