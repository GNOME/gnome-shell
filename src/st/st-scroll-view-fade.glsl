/*
 * st-scroll-view-fade.glsl: Edge fade effect for StScrollView
 *
 * Copyright 2010 Intel Corporation.
 * Copyright 2011 Adel Gadllah
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

uniform sampler2D tex;
uniform float height;
uniform float width;
uniform float fade_offset_top;
uniform float fade_offset_bottom;
uniform float fade_offset_left;
uniform float fade_offset_right;
uniform bool  fade_edges_top;
uniform bool  fade_edges_right;
uniform bool  fade_edges_bottom;
uniform bool  fade_edges_left;
uniform bool  extend_fade_area;

uniform vec2 fade_area_topleft;
uniform vec2 fade_area_bottomright;

void main ()
{
    cogl_color_out = cogl_color_in * texture2D (tex, vec2 (cogl_tex_coord_in[0].xy));

    float y = height * cogl_tex_coord_in[0].y;
    float x = width * cogl_tex_coord_in[0].x;
    float ratio = 1.0;

    if (x > fade_area_topleft[0] && x < fade_area_bottomright[0] &&
        y > fade_area_topleft[1] && y < fade_area_bottomright[1])
    {
        float fade_top_start = fade_area_topleft[1] + fade_offset_top;
        float fade_left_start = fade_area_topleft[0] + fade_offset_left;
        float fade_bottom_start = fade_area_bottomright[1] - fade_offset_bottom;
        float fade_right_start = fade_area_bottomright[0] - fade_offset_right;
        bool fade_top = y < fade_top_start && fade_edges_top;
        bool fade_bottom = y > fade_bottom_start && fade_edges_bottom;
        bool fade_left = x < fade_left_start && fade_edges_left;
        bool fade_right = x > fade_right_start && fade_edges_right;

        if (fade_top) {
            ratio *= (fade_area_topleft[1] - y) / (fade_area_topleft[1] - fade_top_start);
        }

        if (fade_bottom) {
            ratio *= (fade_area_bottomright[1] - y) / (fade_area_bottomright[1] - fade_bottom_start);
        }

        if (fade_left) {
            ratio *= (fade_area_topleft[0] - x) / (fade_area_topleft[0] - fade_left_start);
        }

        if (fade_right) {
            ratio *= (fade_area_bottomright[0] - x) / (fade_area_bottomright[0] - fade_right_start);
        }
    } else if (extend_fade_area) {
        if (x <= fade_area_topleft[0] && fade_edges_left ||
            x >= fade_area_bottomright[0] && fade_edges_right ||
            y <= fade_area_topleft[1] && fade_edges_top ||
            y >= fade_area_bottomright[1] && fade_edges_bottom) {
            ratio = 0.0;
        }
    }

    cogl_color_out *= ratio;
}
