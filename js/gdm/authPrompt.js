// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Signals = imports.signals;
const St = imports.gi.St;

const Animation = imports.ui.animation;
const ShellEntry = imports.ui.shellEntry;
const Tweener = imports.ui.tweener;
const UserWidget = imports.ui.userWidget;

const DEFAULT_BUTTON_WELL_ICON_SIZE = 24;
const DEFAULT_BUTTON_WELL_ANIMATION_DELAY = 1.0;
const DEFAULT_BUTTON_WELL_ANIMATION_TIME = 0.3;

const AuthPrompt = new Lang.Class({
    Name: 'AuthPrompt',

    _init: function() {
        this.actor = new St.BoxLayout({ style_class: 'login-dialog-prompt-layout',
                                        vertical: true });
        this.actor.connect('key-press-event',
                           Lang.bind(this, function(actor, event) {
                               if (event.get_key_symbol() == Clutter.KEY_Escape) {
                                   this.emit('cancel');
                               }
                           }));

        this._userWell = new St.Bin({ x_fill: true,
                                      x_align: St.Align.START });
        this.actor.add(this._userWell,
                       { x_align: St.Align.START,
                         x_fill: true,
                         y_fill: true,
                         expand: true });
        this._label = new St.Label({ style_class: 'login-dialog-prompt-label' });

        this.actor.add(this._label,
                       { expand: true,
                         x_fill: true,
                         y_fill: true,
                         x_align: St.Align.START });
        this._entry = new St.Entry({ style_class: 'login-dialog-prompt-entry',
                                     can_focus: true });
        ShellEntry.addContextMenu(this._entry, { isPassword: true });

        this.actor.add(this._entry,
                       { expand: true,
                         x_fill: true,
                         y_fill: false,
                         x_align: St.Align.START });

        this._entry.grab_key_focus();

        this._message = new St.Label({ opacity: 0 });
        this.actor.add(this._message, { x_fill: true });

        this._loginHint = new St.Label({ style_class: 'login-dialog-prompt-login-hint-message' });
        this.actor.add(this._loginHint);

        this._buttonBox = new St.BoxLayout({ style_class: 'login-dialog-button-box',
                                             vertical: false });
        this.actor.add(this._buttonBox,
                       { expand:  true,
                         x_align: St.Align.MIDDLE,
                         y_align: St.Align.END });

        this._defaultButtonWell = new St.Widget();
        this._defaultButtonWellActor = null;

        this._initButtons();

        let spinnerIcon = global.datadir + '/theme/process-working.svg';
        this._spinner = new Animation.AnimatedIcon(spinnerIcon, DEFAULT_BUTTON_WELL_ICON_SIZE);
        this._spinner.actor.opacity = 0;
        this._spinner.actor.show();
        this._defaultButtonWell.add_child(this._spinner.actor);
    },

    _initButtons: function() {
        this.cancelButton = new St.Button({ style_class: 'modal-dialog-button',
                                            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
                                            reactive: true,
                                            can_focus: true,
                                            label: _("Cancel") });
        this.cancelButton.connect('clicked',
                                   Lang.bind(this, function() {
                                       this.emit('cancel');
                                   }));
        this._buttonBox.add(this.cancelButton,
                            { expand: false,
                              x_fill: false,
                              y_fill: false,
                              x_align: St.Align.START,
                              y_align: St.Align.END });

        this._buttonBox.add(this._defaultButtonWell,
                            { expand: true,
                              x_fill: false,
                              y_fill: false,
                              x_align: St.Align.END,
                              y_align: St.Align.MIDDLE });
        this.nextButton = new St.Button({ style_class: 'modal-dialog-button',
                                          button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
                                          reactive: true,
                                          can_focus: true,
                                          label: _("Next") });
        this.nextButton.connect('clicked',
                                 Lang.bind(this, function() {
                                     this.emit('next');
                                 }));
        this.nextButton.add_style_pseudo_class('default');
        this._buttonBox.add(this.nextButton,
                            { expand: false,
                              x_fill: false,
                              y_fill: false,
                              x_align: St.Align.END,
                              y_align: St.Align.END });

        this._updateNextButtonSensitivity(this._entry.text.length > 0);

        this._entry.clutter_text.connect('text-changed',
                                         Lang.bind(this, function() {
                                             this._updateNextButtonSensitivity(this._entry.text.length > 0);
                                         }));
        this._entry.clutter_text.connect('activate', Lang.bind(this, function() {
            this.emit('next');
        }));
    },

    addActorToDefaultButtonWell: function(actor) {
        this._defaultButtonWell.add_child(actor);

        actor.add_constraint(new Clutter.AlignConstraint({ source: this._spinner.actor,
                                                           align_axis: Clutter.AlignAxis.BOTH,
                                                           factor: 0.5 }));
    },

    setActorInDefaultButtonWell: function(actor, animate) {
        if (!this._defaultButtonWellActor &&
            !actor)
            return;

        let oldActor = this._defaultButtonWellActor;

        if (oldActor)
            Tweener.removeTweens(oldActor);

        let isSpinner;
        if (actor == this._spinner.actor)
            isSpinner = true;
        else
            isSpinner = false;

        if (this._defaultButtonWellActor != actor && oldActor) {
            if (!animate) {
                oldActor.opacity = 0;
            } else {
                Tweener.addTween(oldActor,
                                 { opacity: 0,
                                   time: DEFAULT_BUTTON_WELL_ANIMATION_TIME,
                                   delay: DEFAULT_BUTTON_WELL_ANIMATION_DELAY,
                                   transition: 'linear',
                                   onCompleteScope: this,
                                   onComplete: function() {
                                      if (isSpinner) {
                                          if (this._spinner)
                                              this._spinner.stop();
                                      }
                                   }
                                 });
            }
        }

        if (actor) {
            if (isSpinner)
                this._spinner.play();

            if (!animate)
                actor.opacity = 255;
            else
                Tweener.addTween(actor,
                                 { opacity: 255,
                                   time: DEFAULT_BUTTON_WELL_ANIMATION_TIME,
                                   delay: DEFAULT_BUTTON_WELL_ANIMATION_DELAY,
                                   transition: 'linear' });
        }

        this._defaultButtonWellActor = actor;
    },

    startSpinning: function() {
        this.setActorInDefaultButtonWell(this._spinner.actor, true);
    },

    stopSpinning: function() {
        this.setActorInDefaultButtonWell(null, false);
    },

    clear: function() {
        this._entry.text = '';
        this.stopSpinning();
    },

    setPasswordChar: function(passwordChar) {
        this._entry.clutter_text.set_password_char(passwordChar);
        this._entry.menu.isPassword = passwordChar != '';
    },

    setQuestion: function(question) {
        this._label.set_text(question);

        this._label.show();
        this._entry.show();

        this._loginHint.opacity = 0;
        this._loginHint.show();

        this._entry.grab_key_focus();
    },

    getAnswer: function() {
        let text = this._entry.get_text();

        return text;
    },

    setMessage: function(message, styleClass) {
        if (message) {
            this._message.text = message;
            this._message.styleClass = styleClass;
            this._message.opacity = 255;
        } else {
            this._message.opacity = 0;
        }
    },

    _updateNextButtonSensitivity: function(sensitive) {
        this.nextButton.reactive = sensitive;
        this.nextButton.can_focus = sensitive;
    },

    updateSensitivity: function(sensitive) {
        this._updateNextButtonSensitivity(sensitive);
        this._entry.reactive = sensitive;
        this._entry.clutter_text.editable = sensitive;
    },

    hide: function() {
        this.setActorInDefaultButtonWell(null, true);
        this.actor.hide();
        this._loginHint.opacity = 0;

        this.setUser(null);

        this.updateSensitivity(true);
        this._entry.set_text('');
    },

    setUser: function(user) {
        if (user) {
            let userWidget = new UserWidget.UserWidget(user);
            this._userWell.set_child(userWidget.actor);
        } else {
            this._userWell.set_child(null);
        }
    },

    setHint: function(message) {
        if (message) {
            this._loginHint.set_text(message)
            this._loginHint.opacity = 255;
        } else {
            this._loginHint.opacity = 0;
            this._loginHint.set_text('');
        }
    },

    reset: function() {
        this._message.opacity = 0;
        this.setUser(null);
        this.stopSpinning();
    },

    addCharacter: function(unichar) {
        if (!this._entry.visible)
            return;

        this._entry.grab_key_focus();
        this._entry.clutter_text.insert_unichar(unichar);
    }
});
Signals.addSignalMethods(AuthPrompt.prototype);
