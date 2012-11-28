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
uniform float offset_bottom;
uniform float offset_top;
uniform float offset_right;
uniform float offset_left;

/*
 * Used to pass the fade area to the shader
 *
 * [0][0] = x1
 * [0][1] = y1
 * [1][0] = x2
 * [1][1] = y2
 *
 */
uniform mat2 fade_area;

void main ()
{
    cogl_color_out = cogl_color_in * texture2D (tex, vec2 (cogl_tex_coord_in[0].xy));

    float y = height * cogl_tex_coord_in[0].y;
    float x = width * cogl_tex_coord_in[0].x;

    if (x < fade_area[0][0] || x > fade_area[1][0] ||
        y < fade_area[0][1] || y > fade_area[1][1])
        return;

    float ratio = 1.0;
    float fade_bottom_start = fade_area[1][1] - offset_bottom;
    float fade_right_start = fade_area[1][0] - offset_right;
    float ratio_top = y / offset_top;
    float ratio_bottom = (fade_area[1][1] - y)/(fade_area[1][1] - fade_bottom_start);
    float ratio_left = x / offset_left;
    float ratio_right = (fade_area[1][0] - x)/(fade_area[1][0] - fade_right_start);
    bool in_scroll_area = fade_area[0][0] <= x && fade_area[1][0] >= x;
    bool fade_top = y < offset_top;
    bool fade_bottom = y > fade_bottom_start;
    bool fade_left = x < offset_left;
    bool fade_right = x > fade_right_start;

    if (fade_top) {
        ratio *= ratio_top;
    }

    if (fade_bottom) {
        ratio *= ratio_bottom;
    }

    if (fade_left) {
        ratio *= ratio_left;
    }

    if (fade_right) {
        ratio *= ratio_right;
    }

    cogl_color_out *= ratio;
}
