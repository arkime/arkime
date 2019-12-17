/*
 * This is a hack to work around a limitation of gtkdoc-scan: it insists
 * on putting () behind every symbol listed in gobject.types. Thus we
 * can't put G_TYPE_OBJECT there, but have to sneak a g_object_get_type()
 * function in the generated source via an #include.
 */
GType 
g_object_get_type (void)
{
  return G_TYPE_OBJECT;
}
