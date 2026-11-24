ObjectCount = 40000
ObjectCounter = 0

local bound = 2

function createObject()
	local degree = rand(0, 6.283185307)
	local length = rand(1.0, 2.0)

	local x = math.cos(degree)
	local y = math.sin(degree)

	local vx = y
	local vy = -x

	pd.createObject(
		x * length,
		y * length
	)
end

for i = 1, ObjectCount do
	createObject()
	--pd.createObject(rand(-bound, bound), rand(-bound, bound))
end