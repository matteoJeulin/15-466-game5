.PHONY : all

#n.b. the '-y' sets autoexec scripts to 'on' so that driver expressions will work
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	BLENDER = /Applications/Blender.app/Contents/MacOS/Blender -y
else ifeq ($(UNAME_S),Linux)
	BLENDER = /snap/bin/blender -y
else
	BLENDER = blender
endif

EXPORT_MESHES=export-meshes.py

EXPORT_MESHES=export-meshes.py
# EXPORT_WALKMESHES=export-walkmeshes.py
EXPORT_SCENE=export-scene.py

DIST=../dist

all : \
	$(DIST)/pong.pnct \
	$(DIST)/pong.scene \
# 	$(DIST)/pong.w \

$(DIST)/pong.pnct : pong.blend $(EXPORT_MESHES)
	$(BLENDER) --background --python $(EXPORT_MESHES) -- '$<':Main '$@'

$(DIST)/pong.scene : pong.blend $(EXPORT_SCENE)
	$(BLENDER) --background --python $(EXPORT_SCENE) -- '$<':Main '$@'

# $(DIST)/pong.w : pong.blend $(EXPORT_WALKMESHES)
# 	$(BLENDER) --background --python $(EXPORT_WALKMESHES) -- '$<':WalkMeshes '$@'
