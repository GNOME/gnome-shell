/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009,2010,2012 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 *
 * Authors:
 *   Havoc Pennington <hp@pobox.com> for litl
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef _COGL_MATRIX_STACK_H_
#define _COGL_MATRIX_STACK_H_

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#include "cogl-matrix.h"
#include "cogl-context.h"


/**
 * SECTION:cogl-matrix-stack
 * @short_description: Functions for efficiently tracking many
 *                     related transformations
 *
 * Matrices can be used (for example) to describe the model-view
 * transforms of objects, texture transforms, and projective
 * transforms.
 *
 * The #CoglMatrix api provides a good way to manipulate individual
 * matrices representing a single transformation but if you need to
 * track many-many such transformations for many objects that are
 * organized in a scenegraph for example then using a separate
 * #CoglMatrix for each object may not be the most efficient way.
 *
 * A #CoglMatrixStack enables applications to track lots of
 * transformations that are related to each other in some kind of
 * hierarchy.  In a scenegraph for example if you want to know how to
 * transform a particular node then you usually have to walk up
 * through the ancestors and accumulate their transforms before
 * finally applying the transform of the node itself. In this model
 * things are grouped together spatially according to their ancestry
 * and all siblings with the same parent share the same initial
 * transformation. The #CoglMatrixStack API is suited to tracking lots
 * of transformations that fit this kind of model.
 *
 * Compared to using the #CoglMatrix api directly to track many
 * related transforms, these can be some advantages to using a
 * #CoglMatrixStack:
 * <itemizedlist>
 *   <listitem>Faster equality comparisons of transformations</listitem>
 *   <listitem>Efficient comparisons of the differences between arbitrary
 *   transformations</listitem>
 *   <listitem>Avoid redundant arithmetic related to common transforms
 *   </listitem>
 *   <listitem>Can be more space efficient (not always though)</listitem>
 * </itemizedlist>
 *
 * For reference (to give an idea of when a #CoglMatrixStack can
 * provide a space saving) a #CoglMatrix can be expected to take 72
 * bytes whereas a single #CoglMatrixEntry in a #CoglMatrixStack is
 * currently around 32 bytes on a 32bit CPU or 36 bytes on a 64bit
 * CPU. An entry is needed for each individual operation applied to
 * the stack (such as rotate, scale, translate) so if most of your
 * leaf node transformations only need one or two simple operations
 * relative to their parent then a matrix stack will likely take less
 * space than having a #CoglMatrix for each node.
 *
 * Even without any space saving though the ability to perform fast
 * comparisons and avoid redundant arithmetic (especially sine and
 * cosine calculations for rotations) can make using a matrix stack
 * worthwhile.
 */

/**
 * CoglMatrixStack:
 *
 * Tracks your current position within a hierarchy and lets you build
 * up a graph of transformations as you traverse through a hierarchy
 * such as a scenegraph.
 *
 * A #CoglMatrixStack always maintains a reference to a single
 * transformation at any point in time, representing the
 * transformation at the current position in the hierarchy. You can
 * get a reference to the current transformation by calling
 * cogl_matrix_stack_get_entry().
 *
 * When a #CoglMatrixStack is first created with
 * cogl_matrix_stack_new() then it is conceptually positioned at the
 * root of your hierarchy and the current transformation simply
 * represents an identity transformation.
 *
 * As you traverse your object hierarchy (your scenegraph) then you
 * should call cogl_matrix_stack_push() whenever you move down one
 * level and call cogl_matrix_stack_pop() whenever you move back up
 * one level towards the root.
 *
 * At any time you can apply a set of operations, such as "rotate",
 * "scale", "translate" on top of the current transformation of a
 * #CoglMatrixStack using functions such as
 * cogl_matrix_stack_rotate(), cogl_matrix_stack_scale() and
 * cogl_matrix_stack_translate(). These operations will derive a new
 * current transformation and will never affect a transformation
 * that you have referenced using cogl_matrix_stack_get_entry().
 *
 * Internally applying operations to a #CoglMatrixStack builds up a
 * graph of #CoglMatrixEntry structures which each represent a single
 * immutable transform.
 */
typedef struct _CoglMatrixStack CoglMatrixStack;

/**
 * CoglMatrixEntry:
 *
 * Represents a single immutable transformation that was retrieved
 * from a #CoglMatrixStack using cogl_matrix_stack_get_entry().
 *
 * Internally a #CoglMatrixEntry represents a single matrix
 * operation (such as "rotate", "scale", "translate") which is applied
 * to the transform of a single parent entry.
 *
 * Using the #CoglMatrixStack api effectively builds up a graph of
 * these immutable #CoglMatrixEntry structures whereby operations
 * that can be shared between multiple transformations will result
 * in shared #CoglMatrixEntry nodes in the graph.
 *
 * When a #CoglMatrixStack is first created it references one
 * #CoglMatrixEntry that represents a single "load identity"
 * operation. This serves as the root entry and all operations
 * that are then applied to the stack will extend the graph
 * starting from this root "load identity" entry.
 *
 * Given the typical usage model for a #CoglMatrixStack and the way
 * the entries are built up while traversing a scenegraph then in most
 * cases where an application is interested in comparing two
 * transformations for equality then it is enough to simply compare
 * two #CoglMatrixEntry pointers directly. Technically this can lead
 * to false negatives that could be identified with a deeper
 * comparison but often these false negatives are unlikely and
 * don't matter anyway so this enables extremely cheap comparisons.
 *
 * <note>#CoglMatrixEntry<!-- -->s are reference counted using
 * cogl_matrix_entry_ref() and cogl_matrix_entry_unref() not with
 * cogl_object_ref() and cogl_object_unref().</note>
 */
typedef struct _CoglMatrixEntry CoglMatrixEntry;

/**
 * cogl_matrix_stack_new:
 * @ctx: A #CoglContext
 *
 * Allocates a new #CoglMatrixStack that can be used to build up
 * transformations relating to objects in a scenegraph like hierarchy.
 * (See the description of #CoglMatrixStack and #CoglMatrixEntry for
 * more details of what a matrix stack is best suited for)
 *
 * When a #CoglMatrixStack is first allocated it is conceptually
 * positioned at the root of your scenegraph hierarchy. As you
 * traverse your scenegraph then you should call
 * cogl_matrix_stack_push() whenever you move down a level and
 * cogl_matrix_stack_pop() whenever you move back up a level towards
 * the root.
 *
 * Once you have allocated a #CoglMatrixStack you can get a reference
 * to the current transformation for the current position in the
 * hierarchy by calling cogl_matrix_stack_get_entry().
 *
 * Once you have allocated a #CoglMatrixStack you can apply operations
 * such as rotate, scale and translate to modify the current transform
 * for the current position in the hierarchy by calling
 * cogl_matrix_stack_rotate(), cogl_matrix_stack_scale() and
 * cogl_matrix_stack_translate().
 *
 * Return value: (transfer full): A newly allocated #CoglMatrixStack
 */
CoglMatrixStack *
cogl_matrix_stack_new (CoglContext *ctx);

/**
 * cogl_matrix_stack_push:
 * @stack: A #CoglMatrixStack
 *
 * Saves the current transform and starts a new transform that derives
 * from the current transform.
 *
 * This is usually called while traversing a scenegraph whenever you
 * traverse one level deeper. cogl_matrix_stack_pop() can then be
 * called when going back up one layer to restore the previous
 * transform of an ancestor.
 */
void
cogl_matrix_stack_push (CoglMatrixStack *stack);

/**
 * cogl_matrix_stack_pop:
 * @stack: A #CoglMatrixStack
 *
 * Restores the previous transform that was last saved by calling
 * cogl_matrix_stack_push().
 *
 * This is usually called while traversing a scenegraph whenever you
 * return up one level in the graph towards the root node.
 */
void
cogl_matrix_stack_pop (CoglMatrixStack *stack);

/**
 * cogl_matrix_stack_load_identity:
 * @stack: A #CoglMatrixStack
 *
 * Resets the current matrix to the identity matrix.
 */
void
cogl_matrix_stack_load_identity (CoglMatrixStack *stack);

/**
 * cogl_matrix_stack_scale:
 * @stack: A #CoglMatrixStack
 * @x: Amount to scale along the x-axis
 * @y: Amount to scale along the y-axis
 * @z: Amount to scale along the z-axis
 *
 * Multiplies the current matrix by one that scales the x, y and z
 * axes by the given values.
 */
void
cogl_matrix_stack_scale (CoglMatrixStack *stack,
                         float x,
                         float y,
                         float z);

/**
 * cogl_matrix_stack_translate:
 * @stack: A #CoglMatrixStack
 * @x: Distance to translate along the x-axis
 * @y: Distance to translate along the y-axis
 * @z: Distance to translate along the z-axis
 *
 * Multiplies the current matrix by one that translates along all
 * three axes according to the given values.
 */
void
cogl_matrix_stack_translate (CoglMatrixStack *stack,
                             float x,
                             float y,
                             float z);

/**
 * cogl_matrix_stack_rotate:
 * @stack: A #CoglMatrixStack
 * @angle: Angle in degrees to rotate.
 * @x: X-component of vertex to rotate around.
 * @y: Y-component of vertex to rotate around.
 * @z: Z-component of vertex to rotate around.
 *
 * Multiplies the current matrix by one that rotates the around the
 * axis-vector specified by @x, @y and @z. The rotation follows the
 * right-hand thumb rule so for example rotating by 10 degrees about
 * the axis-vector (0, 0, 1) causes a small counter-clockwise
 * rotation.
 */
void
cogl_matrix_stack_rotate (CoglMatrixStack *stack,
                          float angle,
                          float x,
                          float y,
                          float z);

/**
 * cogl_matrix_stack_rotate_quaternion:
 * @stack: A #CoglMatrixStack
 * @quaternion: A #CoglQuaternion
 *
 * Multiplies the current matrix by one that rotates according to the
 * rotation described by @quaternion.
 */
void
cogl_matrix_stack_rotate_quaternion (CoglMatrixStack *stack,
                                     const CoglQuaternion *quaternion);

/**
 * cogl_matrix_stack_rotate_euler:
 * @stack: A #CoglMatrixStack
 * @euler: A #CoglEuler
 *
 * Multiplies the current matrix by one that rotates according to the
 * rotation described by @euler.
 */
void
cogl_matrix_stack_rotate_euler (CoglMatrixStack *stack,
                                const CoglEuler *euler);

/**
 * cogl_matrix_stack_multiply:
 * @stack: A #CoglMatrixStack
 * @matrix: the matrix to multiply with the current model-view
 *
 * Multiplies the current matrix by the given matrix.
 */
void
cogl_matrix_stack_multiply (CoglMatrixStack *stack,
                            const CoglMatrix *matrix);

/**
 * cogl_matrix_stack_frustum:
 * @stack: A #CoglMatrixStack
 * @left: X position of the left clipping plane where it
 *   intersects the near clipping plane
 * @right: X position of the right clipping plane where it
 *   intersects the near clipping plane
 * @bottom: Y position of the bottom clipping plane where it
 *   intersects the near clipping plane
 * @top: Y position of the top clipping plane where it intersects
 *   the near clipping plane
 * @z_near: The distance to the near clipping plane (Must be positive)
 * @z_far: The distance to the far clipping plane (Must be positive)
 *
 * Replaces the current matrix with a perspective matrix for a given
 * viewing frustum defined by 4 side clip planes that all cross
 * through the origin and 2 near and far clip planes.
 */
void
cogl_matrix_stack_frustum (CoglMatrixStack *stack,
                           float left,
                           float right,
                           float bottom,
                           float top,
                           float z_near,
                           float z_far);

/**
 * cogl_matrix_stack_perspective:
 * @stack: A #CoglMatrixStack
 * @fov_y: Vertical field of view angle in degrees.
 * @aspect: The (width over height) aspect ratio for display
 * @z_near: The distance to the near clipping plane (Must be positive,
 *   and must not be 0)
 * @z_far: The distance to the far clipping plane (Must be positive)
 *
 * Replaces the current matrix with a perspective matrix based on the
 * provided values.
 *
 * <note>You should be careful not to have too great a @z_far / @z_near
 * ratio since that will reduce the effectiveness of depth testing
 * since there wont be enough precision to identify the depth of
 * objects near to each other.</note>
 */
void
cogl_matrix_stack_perspective (CoglMatrixStack *stack,
                               float fov_y,
                               float aspect,
                               float z_near,
                               float z_far);

/**
 * cogl_matrix_stack_orthographic:
 * @stack: A #CoglMatrixStack
 * @x_1: The x coordinate for the first vertical clipping plane
 * @y_1: The y coordinate for the first horizontal clipping plane
 * @x_2: The x coordinate for the second vertical clipping plane
 * @y_2: The y coordinate for the second horizontal clipping plane
 * @near: The <emphasis>distance</emphasis> to the near clipping
 *   plane (will be <emphasis>negative</emphasis> if the plane is
 *   behind the viewer)
 * @far: The <emphasis>distance</emphasis> to the far clipping
 *   plane (will be <emphasis>negative</emphasis> if the plane is
 *   behind the viewer)
 *
 * Replaces the current matrix with an orthographic projection matrix.
 */
void
cogl_matrix_stack_orthographic (CoglMatrixStack *stack,
                                float x_1,
                                float y_1,
                                float x_2,
                                float y_2,
                                float near,
                                float far);

/**
 * cogl_matrix_stack_get_inverse:
 * @stack: A #CoglMatrixStack
 * @inverse: (out): The destination for a 4x4 inverse transformation matrix
 *
 * Gets the inverse transform of the current matrix and uses it to
 * initialize a new #CoglMatrix.
 *
 * Return value: %TRUE if the inverse was successfully calculated or %FALSE
 *   for degenerate transformations that can't be inverted (in this case the
 *   @inverse matrix will simply be initialized with the identity matrix)
 */
CoglBool
cogl_matrix_stack_get_inverse (CoglMatrixStack *stack,
                               CoglMatrix *inverse);

/**
 * cogl_matrix_stack_get_entry:
 * @stack: A #CoglMatrixStack
 *
 * Gets a reference to the current transform represented by a
 * #CoglMatrixEntry pointer.
 *
 * <note>The transform represented by a #CoglMatrixEntry is
 * immutable.</note>
 *
 * <note>#CoglMatrixEntry<!-- -->s are reference counted using
 * cogl_matrix_entry_ref() and cogl_matrix_entry_unref() and you
 * should call cogl_matrix_entry_unref() when you are finished with
 * and entry you get via cogl_matrix_stack_get_entry().</note>
 *
 * Return value: (transfer none): A pointer to the #CoglMatrixEntry
 *               representing the current matrix stack transform.
 */
CoglMatrixEntry *
cogl_matrix_stack_get_entry (CoglMatrixStack *stack);

/**
 * cogl_matrix_stack_get:
 * @stack: A #CoglMatrixStack
 * @matrix: (out): The potential destination for the current matrix
 *
 * Resolves the current @stack transform into a #CoglMatrix by
 * combining the operations that have been applied to build up the
 * current transform.
 *
 * There are two possible ways that this function may return its
 * result depending on whether the stack is able to directly point
 * to an internal #CoglMatrix or whether the result needs to be
 * composed of multiple operations.
 *
 * If an internal matrix contains the required result then this
 * function will directly return a pointer to that matrix, otherwise
 * if the function returns %NULL then @matrix will be initialized
 * to match the current transform of @stack.
 *
 * <note>@matrix will be left untouched if a direct pointer is
 * returned.</note>
 *
 * Return value: A direct pointer to the current transform or %NULL
 *               and in that case @matrix will be initialized with
 *               the value of the current transform.
 */
CoglMatrix *
cogl_matrix_stack_get (CoglMatrixStack *stack,
                       CoglMatrix *matrix);

/**
 * cogl_matrix_entry_get:
 * @entry: A #CoglMatrixEntry
 * @matrix: (out): The potential destination for the transform as
 *                 a matrix
 *
 * Resolves the current @entry transform into a #CoglMatrix by
 * combining the sequence of operations that have been applied to
 * build up the current transform.
 *
 * There are two possible ways that this function may return its
 * result depending on whether it's possible to directly point
 * to an internal #CoglMatrix or whether the result needs to be
 * composed of multiple operations.
 *
 * If an internal matrix contains the required result then this
 * function will directly return a pointer to that matrix, otherwise
 * if the function returns %NULL then @matrix will be initialized
 * to match the transform of @entry.
 *
 * <note>@matrix will be left untouched if a direct pointer is
 * returned.</note>
 *
 * Return value: A direct pointer to a #CoglMatrix transform or %NULL
 *               and in that case @matrix will be initialized with
 *               the effective transform represented by @entry.
 */
CoglMatrix *
cogl_matrix_entry_get (CoglMatrixEntry *entry,
                       CoglMatrix *matrix);

/**
 * cogl_matrix_stack_set:
 * @stack: A #CoglMatrixStack
 * @matrix: A #CoglMatrix replace the current matrix value with
 *
 * Replaces the current @stack matrix value with the value of @matrix.
 * This effectively discards any other operations that were applied
 * since the last time cogl_matrix_stack_push() was called or since
 * the stack was initialized.
 */
void
cogl_matrix_stack_set (CoglMatrixStack *stack,
                       const CoglMatrix *matrix);

/**
 * cogl_is_matrix_stack:
 * @object: a #CoglObject
 *
 * Determines if the given #CoglObject refers to a #CoglMatrixStack.
 *
 * Return value: %TRUE if @object is a #CoglMatrixStack, otherwise
 *               %FALSE.
 */
CoglBool
cogl_is_matrix_stack (void *object);

/**
 * cogl_matrix_entry_calculate_translation:
 * @entry0: The first reference transform
 * @entry1: A second reference transform
 * @x: (out): The destination for the x-component of the translation
 * @y: (out): The destination for the y-component of the translation
 * @z: (out): The destination for the z-component of the translation
 *
 * Determines if the only difference between two transforms is a
 * translation and if so returns what the @x, @y, and @z components of
 * the translation are.
 *
 * If the difference between the two translations involves anything
 * other than a translation then the function returns %FALSE.
 *
 * Return value: %TRUE if the only difference between the transform of
 *                @entry0 and the transform of @entry1 is a translation,
 *                otherwise %FALSE.
 */
CoglBool
cogl_matrix_entry_calculate_translation (CoglMatrixEntry *entry0,
                                         CoglMatrixEntry *entry1,
                                         float *x,
                                         float *y,
                                         float *z);

/**
 * cogl_matrix_entry_is_identity:
 * @entry: A #CoglMatrixEntry
 *
 * Determines whether @entry is known to represent an identity
 * transform.
 *
 * If this returns %TRUE then the entry is definitely the identity
 * matrix. If it returns %FALSE it may or may not be the identity
 * matrix but no expensive comparison is performed to verify it.
 *
 * Return value: %TRUE if @entry is definitely an identity transform,
 *               otherwise %FALSE.
 */
CoglBool
cogl_matrix_entry_is_identity (CoglMatrixEntry *entry);

/**
 * cogl_matrix_entry_equal:
 * @entry0: The first #CoglMatrixEntry to compare
 * @entry1: A second #CoglMatrixEntry to compare
 *
 * Compares two arbitrary #CoglMatrixEntry transforms for equality
 * returning %TRUE if they are equal or %FALSE otherwise.
 *
 * <note>In many cases it is unnecessary to use this api and instead
 * direct pointer comparisons of entries are good enough and much
 * cheaper too.</note>
 *
 * Return value: %TRUE if @entry0 represents the same transform as
 *               @entry1, otherwise %FALSE.
 */
CoglBool
cogl_matrix_entry_equal (CoglMatrixEntry *entry0,
                         CoglMatrixEntry *entry1);

/**
 * cogl_debug_matrix_entry_print:
 * @entry: A #CoglMatrixEntry
 *
 * Allows visualizing the operations that build up the given @entry
 * for debugging purposes by printing to stdout.
 */
void
cogl_debug_matrix_entry_print (CoglMatrixEntry *entry);

/**
 * cogl_matrix_entry_ref:
 * @entry: A #CoglMatrixEntry
 *
 * Takes a reference on the given @entry to ensure the @entry stays
 * alive and remains valid. When you are finished with the @entry then
 * you should call cogl_matrix_entry_unref().
 *
 * It is an error to pass an @entry pointer to cogl_object_ref() and
 * cogl_object_unref()
 */
CoglMatrixEntry *
cogl_matrix_entry_ref (CoglMatrixEntry *entry);

/**
 * cogl_matrix_entry_unref:
 * @entry: A #CoglMatrixEntry
 *
 * Releases a reference on @entry either taken by calling
 * cogl_matrix_entry_unref() or to release the reference given when
 * calling cogl_matrix_stack_get_entry().
 */
void
cogl_matrix_entry_unref (CoglMatrixEntry *entry);

#endif /* _COGL_MATRIX_STACK_H_ */
