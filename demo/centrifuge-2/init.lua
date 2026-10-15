ObjectCount = 3000
ObjectCounter = 0

FrameCounter = 0
GravityIncrement = 0.0009 / 1800
AngularAcceleration = 0.00004

for i = 1, 2500 do
	pd.createObject(math.random() * 2 - 1, math.random() * 2 - 1)
end