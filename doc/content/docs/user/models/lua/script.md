simulation
channel physical

uses
dse.modelc https://github.com/boschglobal/dse.modelc v2.1.32

model lua_inst dse.modelc.lua path=data/model.lua
channel physical signal
file lua_model.lua data/model.lua