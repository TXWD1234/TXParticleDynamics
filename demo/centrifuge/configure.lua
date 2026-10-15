re.config.Isometric = true
pd.param.SubstepCount = 3


function rand(min, max)
	return math.random() * (max - min) + min
end

function createObject()
	local degree = rand(0, 6.283185307)
	local length = rand(8, 16)

	local x = math.cos(degree)
	local y = math.sin(degree)

	local vx = y
	local vy = -x

	pd.createObject(
		x * length,
		y * length,
		vx * length * 0.005,
		vy * length * 0.005
	)
end