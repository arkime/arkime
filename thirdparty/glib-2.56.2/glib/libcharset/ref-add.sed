/^# Packages using this file: / {
  s/# Packages using this file://
  ta
  :a
  s/ glib / glib /
  tb
  s/ $/ glib /
  :b
  s/^/# Packages using this file:/
}
