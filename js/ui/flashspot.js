// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;

const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;
const Tweener = imports.ui.tweener;

const FLASHSPOT_ANIMATION_TIME = 0.25; // seconds

const Flashspot = new Lang.Class({
    Name: 'Flashspot',
    Extends: Lightbox.Lightbox,

    _init: function(area) {
        this.parent(Main.uiGroup, { inhibitEvents: true,
                                    width: area.width,
                                    height: area.height });

        this.actor.style_class = 'flashspot';
        this.actor.set_position(area.x, area.y);
    },

    fire: function() {
        this.actor.opacity = 0;
        Tweener.addTween(this.actor,
                         { opacity: 255,
                           time: FLASHSPOT_ANIMATION_TIME,
                           transition: 'linear',
                           onComplete: Lang.bind(this, this._onFireShowComplete)
                         });
        this.actor.show();
    },

    _onFireShowComplete: function() {
        Tweener.addTween(this.actor,
                         { opacity: 0,
                           time: FLASHSPOT_ANIMATION_TIME,
                           transition: 'linear',
                           onComplete: Lang.bind(this, function() {
                               this.destroy();
                           })
                         });
    }
});
