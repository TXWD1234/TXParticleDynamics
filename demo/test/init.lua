ObjectCounter = 0

local bound = 0.3

-- for i = 1, ObjectCount do
-- 	pd.createObject(rand(-bound, bound), rand(-bound, bound))
-- end

--pd.createObject(0.2, 0.2,  -0.002, -0.002)
--pd.createObject(-0.2, -0.2, 0.002,  0.002)
--pd.createObject(-0.3, -0.3, 0.002,  0.002)

-- pd.createObject(-0.1, -0.1)
-- pd.createObject( 0.1,  0.1)
-- pd.createObject( 0.2 * 2,  0.2 * 2)
-- pd.createObject( 0.03 * 2,  0.03 * 2)
-- pd.createObject( 0.04 * 2,  0.04 * 2)
-- pd.createObject( 0.05 * 2,  0.05 * 2)
-- pd.createObject( 0.06 * 2,  0.06 * 2)
-- pd.createObject( 0.07 * 2,  0.07 * 2)
-- pd.createObject( 0.08 * 2,  0.08 * 2)
-- pd.createObject( 0.09 * 2,  0.09 * 2)

for i = 1, 1024, 1 do
	pd.createObject(rand(-0.9, 0.9), rand(-0.9, 0.9))
end