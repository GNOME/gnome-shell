// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;
const Clutter = imports.gi.Clutter;
const St = imports.gi.St;

function connectLayoutManager(layoutManager, styleChanged) {
    let widget, styleChangedId;

    function _styleChanged() {
        let themeNode = widget.get_theme_node();
        styleChanged(themeNode, widget);
    }

    function actorChanged() {
        if (widget) {
            widget.disconnect(styleChangedId);
            styleChangedId = 0;
        }

        let actor = layoutManager.get_actor();
        if (actor && actor instanceof St.Widget) {
            widget = actor;
            styleChangedId = widget.connect('style-changed', _styleChanged);
            _styleChanged();
        }
    }

    layoutManager.connect('notify::actor', actorChanged);
    return layoutManager;
}

function connectSpacing(layoutManager) {
    return connectLayoutManager(layoutManager, function(themeNode, widget) {
        layoutManager.spacing = themeNode.get_length('spacing');
    });
}

const CenterLayout = new Lang.Class({
    Name: 'CenterLayout',
    Extends: Clutter.BoxLayout,

    vfunc_allocate: function(container, box, flags) {
        let rtl = container.get_text_direction() == Clutter.TextDirection.RTL;

        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;

        // Assume that these are the first three widgets and they are all visible.
        let [left, center, right] = container.get_children();

        // Only support horizontal layouts for now.
        let [centerMinWidth, centerNaturalWidth] = center.get_preferred_width(availWidth);

        let sideWidth = (availWidth - centerNaturalWidth) / 2;

        let childBox = new Clutter.ActorBox();
        childBox.y1 = box.y1;
        childBox.y2 = box.y1 + availHeight;

        if (left) {
            let [leftMinWidth, leftNaturalWidth] = left.get_preferred_width(availWidth);
            let leftSide = Math.min(Math.floor(sideWidth), leftNaturalWidth);
            if (rtl) {
                childBox.x1 = availWidth - leftSide;
                childBox.x2 = availWidth;
            } else {
                childBox.x1 = 0;
                childBox.x2 = leftSide;
            }
            childBox.x1 += box.x1;
            left.allocate(childBox, flags);
        }

        childBox.x1 = box.x1 + Math.ceil(sideWidth);
        childBox.x2 = childBox.x1 + centerNaturalWidth;
        center.allocate(childBox, flags);

        if (right) {
            let [rightMinWidth, rightNaturalWidth] = right.get_preferred_width(availWidth);
            let rightSide = Math.min(Math.floor(sideWidth), rightNaturalWidth);
            if (rtl) {
                childBox.x1 = 0;
                childBox.x2 = rightSide;
            } else {
                childBox.x1 = availWidth - rightSide;
                childBox.x2 = availWidth;
            }
            childBox.x1 += box.x1;
            right.allocate(childBox, flags);
        }
    }
});
