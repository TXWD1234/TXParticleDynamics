pd.param.SubstepCount = 1
pd.param.ObjectRadius = 0.01
--pd.config.Gravity = 0.000004
pd.config.Gravity = 0.0001
pd.param.MaxEntityCount = 160000
pd.config.Restitution = 1.0
pd.config.Friction = 1.0

ObjectCount = 2

function rand(min, max)
	return math.random() * (max - min) + min
end