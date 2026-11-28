pd.param.SubstepCount = 1
pd.param.ObjectRadius = 0.0008
pd.config.Gravity = 0.0000002
--pd.config.Gravity = 0.00000001
--pd.config.Gravity = 0.00004
pd.param.MaxEntityCount = 240000
pd.config.Restitution = 1.0
pd.config.Friction = 0.999

ObjectCount = 2

math.randomseed(65536)
function rand(min, max)
	return math.random() * (max - min) + min
end
function randCoord(range)
	return rand(-range, range), rand(-range, range)
end
function randPolar(min, max)
	local degree = rand(0, 6.283185307)
	local length = rand(min, max)
	return math.cos(degree) * length, math.sin(degree) * length
end