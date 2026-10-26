pd.param.SubstepCount = 4
pd.param.ObjectRadius = 0.0028
re.config.ObjectRadius = 0.0028
pd.config.Gravity = 0.000002
pd.param.MaxEntityCount = 160000
--pd.config.Restitution = 0.9

function rand(min, max)
	return math.random() * (max - min) + min
end