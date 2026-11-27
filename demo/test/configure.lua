pd.param.SubstepCount = 1
pd.param.ObjectRadius = 0.001
pd.config.Gravity = 0.0000000001
--pd.config.Gravity = 0.00000001
--pd.config.Gravity = 0.00004
pd.param.MaxEntityCount = 160000
pd.config.Restitution = 1.0
pd.config.Friction = 0.995

ObjectCount = 2

math.randomseed(65536)
function rand(min, max)
	return math.random() * (max - min) + min
end