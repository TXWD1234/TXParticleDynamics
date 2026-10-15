-- if ObjectCounter < ObjectCount then
-- 	createObject()
-- 	createObject()
-- 	createObject()
-- 	createObject()
-- 	ObjectCounter = ObjectCounter + 4
-- end

pd.applyRotationalForce(AngularAcceleration)
pd.config.Gravity = pd.config.Gravity + GravityIncrement

if FrameCounter == 1800 then
	-- reset
	pd.config.Gravity = 0.0001
	FrameCounter = 0

	-- intensify
	-- AngularAcceleration = AngularAcceleration ^ 0.988
	-- GravityIncrement = GravityIncrement * 1.2
	GravityIncrement = GravityIncrement * 1.18
	AngularAcceleration = AngularAcceleration * 1.086278049
end
FrameCounter = FrameCounter + 1