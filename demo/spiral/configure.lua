pd.config.Gravity = 0.0
pd.config.Friction = 0.98
pd.param.SubstepCount = 3


function rand(min, max)
	return math.random() * (max - min) + min
end