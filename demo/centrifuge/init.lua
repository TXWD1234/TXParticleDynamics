ObjectCount = 3000
ObjectCounter = 0

FrameCounter = 0

for i = 1, 2500 do
	pd.createObject(math.random() * 2 - 1, math.random() * 2 - 1)
end