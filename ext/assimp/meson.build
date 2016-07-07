assimp_sources = [
  'gstassimp.c',
]

assimp = dependency('assimp', required : false)

if assimp.found()
  gstopengl = library('gstassimp',
    assimp_sources,
    c_args : gst_plugins_bad_args,
    link_args : noseh_link_args,
    include_directories : [configinc],
    dependencies : [assimp_dep, gst3d_dep, gstvideo_dep, gstbase_dep, libm],
    install : true,
    install_dir : plugins_install_dir,
  )
endif
