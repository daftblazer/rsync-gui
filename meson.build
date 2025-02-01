project('rsync-gtk', 'c',
  version: '0.1.0',
  meson_version: '>= 0.59.0',
  default_options: ['warning_level=2', 'c_std=gnu11'])

gtkdep = dependency('gtk4')

gnome = import('gnome')

resources = gnome.compile_resources('resources',
  'src/rsync-gtk.gresource.xml',
  source_dir: '.',
  c_name: 'resources')

executable('rsync-gtk',
  'src/main.c',
  'src/window.c',
  'src/window.h',
  'src/progress_window.c',
  'src/progress_window.h',
  resources,
  dependencies: gtkdep,
  install: true)