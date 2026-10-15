---@meta
-- Definition file for lua's intellisense to recognize the APIs

---@class EngineParameter
---@field MaxEntityCount integer
---@field SubstepCount integer

---@class EngineConfig
---@field ObjectRadius number
---@field Gravity number
---@field Friction number
---@field Restitution number

---@class ParticleDynamicsEngine
---@field config EngineConfig
---@field param EngineParameter
---@field createObject fun(x: number, y: number, initVelX: number, initVelY: number)
---@field createObject fun(x: number, y: number)
---@field applyImpulse fun(x: number, y: number, radius: number, intensity: number)
---@field applyRotationalForce fun(magnitude: number, x: number, y: number)
---@field applyRotationalForce fun(magnitude: number)

---@type ParticleDynamicsEngine
pd = {}

---@class RenderEngineParameter

---@class RenderEngineConfig
---@field Isometric boolean
---@field ObjectRadius number

---@class ParticleDynamicsRenderEngine
---@field config RenderEngineConfig
---@field param RenderEngineParameter

---@type ParticleDynamicsRenderEngine
re = {}