#!/usr/bin/env python

# 1 surface wisps with 1 point light
# Copyright (c) 2011-2012 Hiroshi Tsubokawa

import fujiyama

si = fujiyama.SceneInterface()

#plugins
si.OpenPlugin('ConstantShader')
si.OpenPlugin('PlasticShader')
si.OpenPlugin('VolumeShader')
si.OpenPlugin('SurfaceWispsProcedure')

#Camera
si.NewCamera('cam1', 'PerspectiveCamera')
si.SetProperty3('cam1', 'position', 0, 2, 6)
si.SetProperty3('cam1', 'direction', 0, -.2, -1)

#Light
si.NewLight('light1', 'PointLight')
si.SetProperty3('light1', 'position', 10, 12, 10)

#Shader
si.NewShader('floor_shader', 'PlasticShader')
si.SetProperty3('floor_shader', 'diffuse', .2, .25, .3)
si.SetProperty1('floor_shader', 'ior', 2)

si.NewShader('volume_shader', 'VolumeShader')
si.SetProperty3('volume_shader', 'diffuse', .2, .2, .2)

si.NewShader('dome_shader', 'ConstantShader')
si.SetProperty3('dome_shader', 'diffuse', .8, .8, .8)

#Turbulence
si.NewTurbulence('turbulence_data')
si.SetProperty3('turbulence_data', 'frequency', 1.5, 1.5, .5)
si.SetProperty3('turbulence_data', 'amplitude', 1, 1, 1)
si.SetProperty3('turbulence_data', 'offset', .6, 0, 0)

#Volume
si.NewVolume('volume_data')
si.SetProperty3('volume_data', 'bounds_min', -1, -1, -1)
si.SetProperty3('volume_data', 'bounds_max', 1, 1, 1)
#si.SetProperty3('volume_data', 'resolution', 50, 50, 50)
si.SetProperty3('volume_data', 'resolution', 500, 500, 500)

#Procedure
si.NewProcedure('proc1', 'SurfaceWispsProcedure')
si.AssignVolume('proc1', 'volume', 'volume_data')
si.AssignTurbulence('proc1', 'turbulence', 'turbulence_data')
si.RunProcedure('proc1')

#Mesh
si.NewMesh('dome_mesh', '../../mesh/dome.mesh')
si.NewMesh('floor_mesh', '../../mesh/floor.mesh')

#ObjectInstance
si.NewObjectInstance('volume1', 'volume_data')
si.AssignShader('volume1', 'volume_shader')
si.SetProperty3('volume1', 'translate', 0, .75, 0)

si.NewObjectInstance('floor1', 'floor_mesh')
si.AssignShader('floor1', 'floor_shader')

si.NewObjectInstance('dome1', 'dome_mesh')
si.AssignShader('dome1', 'dome_shader')

#FrameBuffer
si.NewFrameBuffer('fb1', 'rgba')

#Properties
si.ShowPropertyList('SurfaceWispsProcedure')
si.ShowPropertyList('VolumeShader')
si.ShowPropertyList('Renderer')
si.ShowPropertyList('Volume')

#Renderer
si.NewRenderer('ren1')
si.AssignCamera('ren1', 'cam1')
si.AssignFrameBuffer('ren1', 'fb1')
si.SetProperty2('ren1', 'resolution', 640, 480)
#si.SetProperty2('ren1', 'resolution', 160, 120)

si.SetProperty1('ren1', 'raymarch_step', .01)
si.SetProperty1('ren1', 'raymarch_shadow_step', .02)
si.SetProperty1('ren1', 'raymarch_reflect_step', .02)

#Rendering
si.RenderScene('ren1')

#Output
si.SaveFrameBuffer('fb1', '../surface_wisps.fb')

#Run commands
si.Run()
#si.Print()

