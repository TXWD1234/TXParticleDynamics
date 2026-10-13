ObjectCount = 2500
ObjectCounter = 0

FrameCounter = 0

ObjectInitVel = 0.016
Degree = 0.0

for i = 1, ObjectCount do
	pd.createObject(rand(-1.0, 1.0), rand(-1.0, 1.0))
end