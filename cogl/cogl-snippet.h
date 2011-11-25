/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_SNIPPET_H__
#define __COGL_SNIPPET_H__

#include <glib.h>

G_BEGIN_DECLS

/**
 * SECTION:cogl-snippet
 * @short_description: Functions for creating and manipulating shader snippets
 *
 * ...
 */
typedef struct _CoglSnippet CoglSnippet;

#define COGL_SNIPPET(OBJECT) ((CoglSnippet *)OBJECT)

#define cogl_snippet_new cogl_snippet_new_EXP
/**
 * cogl_snippet_new:
 * @declarations: The source code for the declarations for this
 *   snippet or %NULL. See cogl_snippet_set_declarations().
 * @post: The source code to run after the hook point where this
 *   shader snippet is attached or %NULL. See cogl_snippet_set_post().
 *
 * Allocates and initializes a new snippet with the given source strings.
 *
 * Return value: a pointer to a new #CoglSnippet
 *
 * Since: 1.10
 * Stability: Unstable
 */
CoglSnippet *
cogl_snippet_new (const char *declarations,
                  const char *post);

#define cogl_is_snippet cogl_is_snippet_EXP
/**
 * cogl_is_snippet:
 * @handle: A CoglHandle
 *
 * Gets whether the given handle references an existing snippet object.
 *
 * Return value: %TRUE if the handle references a #CoglSnippet,
 *   %FALSE otherwise
 *
 * Since: 1.10
 * Stability: Unstable
 */
gboolean
cogl_is_snippet (void *object);

#define cogl_snippet_set_declarations cogl_snippet_set_declarations_EXP
/**
 * cogl_snippet_set_declarations:
 * @snippet: A #CoglSnippet
 * @declarations: The new source string for the declarations section
 *   of this snippet.
 *
 * Sets a source string that will be inserted in the global scope of
 * the generated shader when this snippet is used on a pipeline. This
 * string is typically used to declare uniforms, attributes or
 * functions that will be used by the other parts of the snippets.
 *
 * This function should only be called before the snippet is attached
 * to its first pipeline. After that the snippet should be considered
 * immutable.
 *
 * Since: 1.10
 * Stability: Unstable
 */
void
cogl_snippet_set_declarations (CoglSnippet *snippet,
                               const char *declarations);

#define cogl_snippet_get_declarations cogl_snippet_get_declarations_EXP
/**
 * cogl_snippet_get_declarations:
 * @snippet: A #CoglSnippet
 *
 * Return value: the source string that was set with
 *   cogl_snippet_set_declarations() or %NULL if none was set.
 *
 * Since: 1.10
 * Stability: Unstable
 */
const char *
cogl_snippet_get_declarations (CoglSnippet *snippet);

#define cogl_snippet_set_pre cogl_snippet_set_pre_EXP
/**
 * cogl_snippet_set_pre:
 * @snippet: A #CoglSnippet
 * @pre: The new source string for the pre section of this snippet.
 *
 * Sets a source string that will be inserted before the hook point in
 * the generated shader for the pipeline that this snippet is attached
 * to. Please see the documentation of each hook point in
 * #CoglPipeline for a description of how this string should be used.
 *
 * This function should only be called before the snippet is attached
 * to its first pipeline. After that the snippet should be considered
 * immutable.
 *
 * Since: 1.10
 * Stability: Unstable
 */
void
cogl_snippet_set_pre (CoglSnippet *snippet,
                      const char *pre);

#define cogl_snippet_get_pre cogl_snippet_get_pre_EXP
/**
 * cogl_snippet_get_pre:
 * @snippet: A #CoglSnippet
 *
 * Return value: the source string that was set with
 *   cogl_snippet_set_pre() or %NULL if none was set.
 *
 * Since: 1.10
 * Stability: Unstable
 */
const char *
cogl_snippet_get_pre (CoglSnippet *snippet);

#define cogl_snippet_set_replace cogl_snippet_set_replace_EXP
/**
 * cogl_snippet_set_replace:
 * @snippet: A #CoglSnippet
 * @replace: The new source string for the replace section of this snippet.
 *
 * Sets a source string that will be used instead of any generated
 * source code or any previous snippets for this hook point. Please
 * see the documentation of each hook point in #CoglPipeline for a
 * description of how this string should be used.
 *
 * This function should only be called before the snippet is attached
 * to its first pipeline. After that the snippet should be considered
 * immutable.
 *
 * Since: 1.10
 * Stability: Unstable
 */
void
cogl_snippet_set_replace (CoglSnippet *snippet,
                          const char *replace);

#define cogl_snippet_get_replace cogl_snippet_get_replace_EXP
/**
 * cogl_snippet_get_replace:
 * @snippet: A #CoglSnippet
 *
 * Return value: the source string that was set with
 *   cogl_snippet_set_replace() or %NULL if none was set.
 *
 * Since: 1.10
 * Stability: Unstable
 */
const char *
cogl_snippet_get_replace (CoglSnippet *snippet);

#define cogl_snippet_set_post cogl_snippet_set_post_EXP
/**
 * cogl_snippet_set_post:
 * @snippet: A #CoglSnippet
 * @post: The new source string for the post section of this snippet.
 *
 * Sets a source string that will be inserted after the hook point in
 * the generated shader for the pipeline that this snippet is attached
 * to. Please see the documentation of each hook point in
 * #CoglPipeline for a description of how this string should be used.
 *
 * This function should only be called before the snippet is attached
 * to its first pipeline. After that the snippet should be considered
 * immutable.
 *
 * Since: 1.10
 * Stability: Unstable
 */
void
cogl_snippet_set_post (CoglSnippet *snippet,
                       const char *post);

#define cogl_snippet_get_post cogl_snippet_get_post_EXP
/**
 * cogl_snippet_get_post:
 * @snippet: A #CoglSnippet
 *
 * Return value: the source string that was set with
 *   cogl_snippet_set_post() or %NULL if none was set.
 *
 * Since: 1.10
 * Stability: Unstable
 */
const char *
cogl_snippet_get_post (CoglSnippet *snippet);

G_END_DECLS

#endif /* __COGL_SNIPPET_H__ */
