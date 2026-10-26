ObjectCount = 64000
ObjectCounter = 0

local bound = 1

for i = 1, ObjectCount do
	pd.createObject(rand(-bound, bound), rand(-bound, bound))
end