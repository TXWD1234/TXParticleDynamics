if FrameCounter % 180 == 0 then
	pd.config.Gravity = 0.0025
else
	pd.config.Gravity = 0.0001
end

FrameCounter = FrameCounter + 1