project('gst-template', 'c', version : '1.14.0.1', license : 'LGPL')

plugins_install_dir = join_paths(get_option('libdir'), 'gstreamer-1.0')

cc = meson.get_compiler('c')

gst_version = meson.project_version()

api_version = '1.0'

gst_dep = dependency('gstreamer-1.0', version : '>=1.14',
    required : true, fallback : ['gstreamer', 'gst_dep'])
gstbase_dep = dependency('gstreamer-base-1.0', version : '>=1.14',
  fallback : ['gstreamer', 'gst_base_dep'])

gstvideo_dep = dependency('gstreamer-video-1.0', version : '>=1.14', required : true)


modalai_libs = ['modal_json', 'modal_pipe', 'modal_journal']
modlai_deps = []
foreach lib_name : modalai_libs 
    modlai_deps += cc.find_library(lib_name, dirs : ['/usr/lib64'])
endforeach

subdir('gst-app')
subdir('gst-plugin')
