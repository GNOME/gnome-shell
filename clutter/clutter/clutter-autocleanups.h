/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright 2015  Emmanuele Bassi
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __CLUTTER_AUTO_CLEANUPS_H__
#define __CLUTTER_AUTO_CLEANUPS_H__

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __GI_SCANNER__

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterAction, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterActor, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterActorMeta, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterAlignConstraint, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterAnimatable, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterBackend, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterBindConstraint, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterBindingPool, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterBinLayout, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterBlurEffect, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterBoxLayout, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterBrightnessContrastEffect, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterCanvas, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterChildMeta, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterClickAction, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterClone, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterColorizeEffect, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterConstraint, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterContainer, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterContent, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterDeformEffect, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterDesaturateEffect, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterDeviceManager, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterDragAction, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterDropAction, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterEffect, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterFixedLayout, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterFlowLayout, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterGestureAction, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterGridLayout, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterImage, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterInputDevice, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterInterval, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterKeyframeTransition, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterLayoutManager, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterLayoutMeta, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterOffscreenEffect, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterPageTurnEffect, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterPanAction, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterPathConstraint, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterPath, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterPropertyTransition, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterRotateAction, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterScriptable, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterScript, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterScrollActor, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterSettings, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterShaderEffect, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterSnapConstraint, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterStage, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterSwipeAction, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterTapAction, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterTextBuffer, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterText, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterTimeline, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterTransitionGroup, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterTransition, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterZoomAction, g_object_unref)

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterActorBox, clutter_actor_box_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterColor, clutter_color_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterMargin, clutter_margin_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterMatrix, clutter_matrix_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterPaintNode, clutter_paint_node_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterPaintVolume, clutter_paint_volume_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterPathNode, clutter_path_node_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterPoint, clutter_point_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterRect, clutter_rect_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterSize, clutter_size_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClutterVertex, clutter_vertex_free)

#endif /* __GI_SCANNER__ */

#endif /* __CLUTTER_AUTO_CLEANUPS_H__ */
