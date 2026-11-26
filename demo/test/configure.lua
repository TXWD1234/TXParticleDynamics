pd.param.SubstepCount = 4
pd.param.ObjectRadius = 0.01
pd.config.Gravity = 0.0000001
--pd.config.Gravity = 0.00000001
--pd.config.Gravity = 0.00004
pd.param.MaxEntityCount = 160000
pd.config.Restitution = 1.0
pd.config.Friction = 0.99

ObjectCount = 2

function rand(min, max)
	return math.random() * (max - min) + min
end