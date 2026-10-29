pd.param.SubstepCount = 1
pd.param.ObjectRadius = 0.028
re.config.ObjectRadius = 0.028
--pd.config.Gravity = 0.000004
pd.config.Gravity = 0.00000
pd.param.MaxEntityCount = 160000
--pd.config.Restitution = 0.9

ObjectCount = 2

function rand(min, max)
	return math.random() * (max - min) + min
end