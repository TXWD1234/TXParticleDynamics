if ObjectCounter < ObjectCount then
	pd.createObject(0.0, 0.0, ObjectInitVel * math.cos(Degree), ObjectInitVel * math.sin(Degree))
	pd.createObject(0.0, 0.0, ObjectInitVel * math.cos(Degree + 1.046666667 * 1), ObjectInitVel * math.sin(Degree + 1.046666667 * 1))
	pd.createObject(0.0, 0.0, ObjectInitVel * math.cos(Degree + 1.046666667 * 2), ObjectInitVel * math.sin(Degree + 1.046666667 * 2))
	pd.createObject(0.0, 0.0, ObjectInitVel * math.cos(Degree + 1.046666667 * 3), ObjectInitVel * math.sin(Degree + 1.046666667 * 3))
	pd.createObject(0.0, 0.0, ObjectInitVel * math.cos(Degree + 1.046666667 * 4), ObjectInitVel * math.sin(Degree + 1.046666667 * 4))
	pd.createObject(0.0, 0.0, ObjectInitVel * math.cos(Degree + 1.046666667 * 5), ObjectInitVel * math.sin(Degree + 1.046666667 * 5))
	ObjectCounter = ObjectCounter + 6
	Degree = Degree + 0.01744444444 * 4
end
