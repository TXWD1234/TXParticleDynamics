pd.param.SubstepCount = 1
pd.param.ObjectRadius = 0.0008
--pd.config.Gravity = 0.0000001
--pd.config.Gravity = 0.00000001
pd.config.Gravity = 0.0000004
--pd.config.Gravity = 0.000004
pd.param.MaxEntityCount = 400000
pd.config.Restitution = 1.0
pd.config.Friction = 0.99

ObjectCount = 2

math.randomseed(65536)
function rand(min, max)
	return math.random() * (max - min) + min
end