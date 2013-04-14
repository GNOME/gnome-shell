// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const St = imports.gi.St;

const Lang = imports.lang;
const Layout = imports.ui.layout;
const Main = imports.ui.main;
const Shell = imports.gi.Shell;
const Tweener = imports.ui.tweener;

const FADE_TIME = 0.1;

const XkbHandler = new Lang.Class({
    Name: 'XkbHandler',

    _init: function() {

        global.connect('xkb-state-changed', Lang.bind(this, this._onStateChange));
        this.actor = new St.Widget({ x_expand: true,
                                     y_expand: true,
                                     x_align: Clutter.ActorAlign.END,
                                     y_align: Clutter.ActorAlign.END,
                                     margin_left: 100,
                                     margin_right: 100,
                                     margin_bottom: 100});
        this.actor.add_constraint(new Layout.MonitorConstraint({ primary: true }));
        this._box = new St.BoxLayout({ style_class: 'osd-window',
                                       vertical: true });
        this.actor.add_actor(this._box);

        this._label = new St.Label();
        this._box.add(this._label);

        Main.layoutManager.addChrome(this.actor, { affectsInputRegion: false });

        this.actor.hide();
    },

    _onStateChange: function(obj, latched, locked) {
      mods = [ 'Shift', 'Caps', 'Ctrl', 'Alt', 'Num Lock', '', 'Super', '' ];
      markup = '';
      for (let i = 0; i < 8; i++) {
        if (locked & (1 << i))
          {
            if (markup != '') markup += ' ';
            markup += '<b>' + mods[i] + '</b>';
          }
        else if (latched & (1 << i))
          {
            if (markup != '') markup += ' ';
            markup += mods[i];
          }
      }

      this._label.clutter_text.set_markup (markup);
      if (latched != 0 || locked != 0)
        {
           this.actor.show();
           this.actor.opacity = 0;
           Tweener.addTween(this.actor,
                            { opacity: 255,
                              time: FADE_TIME,
                              transition: 'easeOutQuad' });
        }
      else
        {
          this.actor.hide();
        }
      log ('xkb state changed, latched: ' + latched + ' locked: ' + locked);
    }
});
