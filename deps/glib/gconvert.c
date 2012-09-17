#include "gstrfuncs.h"
#include "gconvert.h"

gchar*
g_convert_with_fallback (const gchar  *str,
                         gssize        len,            
                         const gchar  *to_codeset,
                         const gchar  *from_codeset,
                         const gchar  *fallback,
                         gsize        *bytes_read,     
                         gsize        *bytes_written,  
                         GError      **error)
{
  return g_strdup(str);
}

gchar*
g_locale_to_utf8 (const gchar  *opsysstring,
                  gssize        len,            
                  gsize        *bytes_read,     
                  gsize        *bytes_written,  
                  GError      **error)
{
  return g_strdup(opsysstring);
}

gchar *
g_filename_display_name (const gchar *filename)
{
  return g_strdup(filename);
}
