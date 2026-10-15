-- if ObjectCounter < ObjectCount then
-- 	createObject()
-- 	createObject()
-- 	createObject()
-- 	createObject()
-- 	ObjectCounter = ObjectCounter + 4
-- end

pd.applyRotationalForce(0.00004)
if FrameCounter == 600 then
	pd.config.Gravity = 0.0004
end
if FrameCounter == 1200 then
	pd.config.Gravity = 0.0008
end
if FrameCounter == 1800 then
	pd.config.Gravity = 0.0010
end
if FrameCounter == 2400 then
	pd.config.Gravity = 0.0001
	FrameCounter = 0
end
FrameCounter = FrameCounter + 1