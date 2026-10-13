if ObjectCounter < ObjectCount then
	pd.createObject(0.9, 0.9, -0.01, rand(-0.001, 0.001))
	pd.createObject(-0.9, -0.9, 0.01, rand(-0.001, 0.001))
	ObjectCounter = ObjectCounter + 2
end
