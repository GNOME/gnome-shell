/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Lang = imports.lang;
const Gio = imports.gi.Gio;
const St = imports.gi.St;

const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;

// GSettings keys
const SETTINGS_SCHEMA = 'org.gnome.desktop.media-handling';
const SETTING_DISABLE_AUTORUN = 'autorun-never';

const HOTPLUG_ICON_SIZE = 16;

// misc utils
function ignoreAutorunForMount(mount) {
    let root = mount.get_root();
    let volume = mount.get_volume();

    if ((root.is_native() && !isMountRootHidden(root)) ||
        (volume && volume.should_automount()))
        return false;

    return true;
}

function isMountRootHidden(root) {
    let path = root.get_path();

    // skip any mounts in hidden directory hierarchies
    return (path.indexOf('/.') != -1);
}

function startAppForMount(app, mount) {
    let files = [];
    let root = mount.get_root();
    files.push(root);

    try {
        app.launch(files, 
                   global.create_app_launch_context())
    } catch (e) {
        log('Unable to launch the application ' + app.get_name()
            + ': ' + e.toString());
    }
}

/******************************************/

function ContentTypeDiscoverer(callback) {
    this._init(callback);
}

ContentTypeDiscoverer.prototype = {
    _init: function(callback) {
        this._callback = callback;
    },

    guessContentTypes: function(mount) {
        // guess mount's content types using GIO
        mount.guess_content_type(false, null,
                                 Lang.bind(this,
                                           this._onContentTypeGuessed));
    },

    _onContentTypeGuessed: function(mount, res) {
        let contentTypes = [];

        try {
            contentTypes = mount.guess_content_type_finish(res);
        } catch (e) {
            log('Unable to guess content types on added mount ' + mount.get_name()
                + ': ' + e.toString());
        }

        // we're not interested in win32 software content types here
        contentTypes = contentTypes.filter(function(type) {
            return (type != 'x-content/win32-software');
        });

        this._callback(mount, contentTypes);
    }
}

function AutorunManager() {
    this._init();
}

AutorunManager.prototype = {
    _init: function() {
        this._volumeMonitor = Gio.VolumeMonitor.get();

        this._volumeMonitor.connect('mount-added',
                                    Lang.bind(this,
                                              this._onMountAdded));
        this._volumeMonitor.connect('mount-removed',
                                    Lang.bind(this,
                                              this._onMountRemoved));

        this._transDispatcher = new AutorunTransientDispatcher();
        this._residentSource = new AutorunResidentSource();
        this._residentSource.connect('destroy', Lang.bind(this,
            function() {
                this._residentSource = new AutorunResidentSource();
            }));

        let mounts = this._volumeMonitor.get_mounts();

        mounts.forEach(Lang.bind(this, function (mount) {
            let discoverer = new ContentTypeDiscoverer(Lang.bind (this, 
                function (mount, contentTypes) {
                    this._residentSource.addMount(mount, contentTypes);
                }));

            discoverer.guessContentTypes(mount);
        }));
    },

    _onMountAdded: function(monitor, mount) {
        // don't do anything if our session is not the currently
        // active one
        if (!Main.automountManager.ckListener.sessionActive)
            return;

        let discoverer = new ContentTypeDiscoverer(Lang.bind (this,
            function (mount, contentTypes) {
                this._transDispatcher.addMount(mount, contentTypes);
                this._residentSource.addMount(mount, contentTypes);
            }));

        discoverer.guessContentTypes(mount);
    },

    _onMountRemoved: function(monitor, mount) {
        this._transDispatcher.removeMount(mount);
        this._residentSource.removeMount(mount);
    },

    ejectMount: function(mount) {
        // TODO: we need to have a StMountOperation here to e.g. trigger
        // shell dialogs when applications are blocking the mount.
        if (mount.can_eject())
            mount.eject_with_operation(0, null, null,
                                       Lang.bind(this, this._onMountEject));
        else
            mount.unmount_with_operation(0, null, null,
                                         Lang.bind(this, this._onMountEject));
    },

    _onMountEject: function(mount, res) {
        try {
            if (mount.can_eject())
                mount.eject_with_operation_finish(res);
            else
                mount.unmount_with_operation_finish(res);
        } catch (e) {
            log('Unable to eject the mount ' + mount.get_name() 
                + ': ' + e.toString());
        }
    },
}

function AutorunResidentSource() {
    this._init();
}

AutorunResidentSource.prototype = {
    __proto__: MessageTray.Source.prototype,

    _init: function() {
        MessageTray.Source.prototype._init.call(this, _('Removable Devices'));

        this._mounts = [];

        this._notification = new AutorunResidentNotification(this);
        this._setSummaryIcon(this.createNotificationIcon(HOTPLUG_ICON_SIZE));
    },

    addMount: function(mount, contentTypes) {
        if (ignoreAutorunForMount(mount))
            return;

        let filtered = this._mounts.filter(function (element) {
            return (element.mount == mount);
        });

        if (filtered.length != 0)
            return;

        let element = { mount: mount, contentTypes: contentTypes };
        this._mounts.push(element);
        this._redisplay();
    },

    removeMount: function(mount) {
        this._mounts =
            this._mounts.filter(function (element) {
                return (element.mount != mount);
            });

        this._redisplay();
    },

    _redisplay: function() {
        if (this._mounts.length == 0) {
            this._notification.destroy();
            this.destroy();

            return;
        }

        this._notification.updateForMounts(this._mounts);

        // add ourselves as a source, and push the notification
        if (!Main.messageTray.contains(this)) {
            Main.messageTray.add(this);
            this.pushNotification(this._notification);
        }
    },

    createNotificationIcon: function(iconSize) {
        return new St.Icon ({ icon_name: 'drive-harddisk',
                              icon_size: iconSize ? iconSize : this.ICON_SIZE });
    }
}

function AutorunResidentNotification(source) {
    this._init(source);
}

AutorunResidentNotification.prototype = {
    __proto__: MessageTray.Notification.prototype,

    _init: function(source) {
        MessageTray.Notification.prototype._init.call(this, source,
                                                      source.title, null,
                                                      { customContent: true });

        // set the notification as resident
        this.setResident(true);

        this._layout = new St.BoxLayout ({ style_class: 'hotplug-resident-box',
                                           vertical: true });

        this.addActor(this._layout,
                      { x_expand: true,
                        x_fill: true });
    },

    updateForMounts: function(mounts) {
        // remove all the layout content
        this._layout.destroy_children();

        for (let idx = 0; idx < mounts.length; idx++) {
            let element = mounts[idx];

            let actor = this._itemForMount(element.mount, element.contentTypes);
            this._layout.add(actor, { x_fill: true,
                                      expand: true });
        }
    },

    _itemForMount: function(mount, contentTypes) {
        let item = new St.BoxLayout();

        // prepare the mount button content
        let mountLayout = new St.BoxLayout();

        let mountIcon = new St.Icon({ gicon: mount.get_icon(),
                                      style_class: 'hotplug-resident-mount-icon' });
        mountLayout.add_actor(mountIcon);

        let labelBin = new St.Bin({ y_align: St.Align.MIDDLE });
        let mountLabel =
            new St.Label({ text: mount.get_name(),
                           style_class: 'hotplug-resident-mount-label',
                           track_hover: true,
                           reactive: true });
        labelBin.add_actor(mountLabel);
        mountLayout.add_actor(labelBin);

        let mountButton = new St.Button({ child: mountLayout,
                                          x_align: St.Align.START,
                                          x_fill: true,
                                          style_class: 'hotplug-resident-mount',
                                          button_mask: St.ButtonMask.ONE });
        item.add(mountButton, { x_align: St.Align.START,
                                expand: true });

        let ejectIcon = 
            new St.Icon({ icon_name: 'media-eject',
                          style_class: 'hotplug-resident-eject-icon' });

        let ejectButton =
            new St.Button({ style_class: 'hotplug-resident-eject-button',
                            button_mask: St.ButtonMask.ONE,
                            child: ejectIcon });
        item.add(ejectButton, { x_align: St.Align.END });

        // TODO: need to do something better here...
        if (!contentTypes.length)
            contentTypes.push('inode/directory');

        // now connect signals
        mountButton.connect('clicked', Lang.bind(this, function(actor, event) {
            let app = Gio.app_info_get_default_for_type(contentTypes[0], false);

            if (app)
                startAppForMount(app, mount);
        }));

        ejectButton.connect('clicked', Lang.bind(this, function() {
            Main.autorunManager.ejectMount(mount);
        }));

        return item;
    },
}

function AutorunTransientDispatcher() {
    this._init();
}

AutorunTransientDispatcher.prototype = {
    _init: function() {
        this._sources = [];
        this._settings = new Gio.Settings({ schema: SETTINGS_SCHEMA });
    },

    _getSourceForMount: function(mount) {
        let filtered =
            this._sources.filter(function (source) {
                return (source.mount == mount);
            });

        // we always make sure not to add two sources for the same
        // mount in addMount(), so it's safe to assume filtered.length
        // is always either 1 or 0.
        if (filtered.length == 1)
            return filtered[0];

        return null;
    },

    addMount: function(mount, contentTypes) {
        // if autorun is disabled globally, return
        if (this._settings.get_boolean(SETTING_DISABLE_AUTORUN))
            return;

        // if the mount doesn't want to be autorun, return
        if (ignoreAutorunForMount(mount))
            return;

        // finally, if we already have a source showing for this 
        // mount, return
        if (this._getSourceForMount(mount))
            return;
     
        // add a new source
        this._sources.push(new AutorunTransientSource(mount, contentTypes));
    },

    removeMount: function(mount) {
        let source = this._getSourceForMount(mount);
        
        // if we aren't tracking this mount, don't do anything
        if (!source)
            return;

        // destroy the notification source
        source.destroy();
    }
}

function AutorunTransientSource(mount, contentTypes) {
    this._init(mount, contentTypes);
}

AutorunTransientSource.prototype = {
    __proto__: MessageTray.Source.prototype,

    _init: function(mount, contentTypes) {
        MessageTray.Source.prototype._init.call(this, mount.get_name());

        this.mount = mount;
        this.contentTypes = contentTypes;

        this._notification = new AutorunTransientNotification(this);
        this._setSummaryIcon(this.createNotificationIcon(this.ICON_SIZE));

        // add ourselves as a source, and popup the notification
        Main.messageTray.add(this);
        this.notify(this._notification);
    },

    createNotificationIcon: function(iconSize) {
        return new St.Icon({ gicon: this.mount.get_icon(),
                             icon_size: iconSize ? iconSize : this.ICON_SIZE });
    }
}

function AutorunTransientNotification(source) {
    this._init(source);
}

AutorunTransientNotification.prototype = {
    __proto__: MessageTray.Notification.prototype,

    _init: function(source) {
        MessageTray.Notification.prototype._init.call(this, source,
                                                      source.title, null,
                                                      { customContent: true });

        this._box = new St.BoxLayout({ style_class: 'hotplug-transient-box',
                                       vertical: true });
        this.addActor(this._box);

        this._mount = source.mount;

        source.contentTypes.forEach(Lang.bind(this, function (type) {
            let actor = this._buttonForContentType(type);

            if (actor)
                this._box.add(actor, { x_fill: true,
                                       x_align: St.Align.START });
        }));

        // TODO: ideally we never want to show the file manager entry here,
        // but we want to detect which kind of files are present on the device,
        // and use those to present a more meaningful choice.
        if (this._contentTypes.length == 0) {
            let button = this._buttonForContentType('inode/directory');

            if (button)
                this._box.add (button, { x_fill: true,
                                         x_align: St.Align.START });
        }

        this._box.add(this._buttonForEject(), { x_fill: true,
                                                x_align: St.Align.START });

        // set the notification to transient and urgent, so that it
        // expands out
        this.setTransient(true);
        this.setUrgency(MessageTray.Urgency.CRITICAL);
    },

    _buttonForContentType: function(type) {
        let app = Gio.app_info_get_default_for_type(type, false);

        if (!app)
            return null;

        let box = new St.BoxLayout();

        let icon = new St.Icon({ gicon: app.get_icon(),
                                 style_class: 'hotplug-notification-item-icon' });
        box.add(icon);

        let label = new St.Bin({ y_align: St.Align.MIDDLE,
                                 child: new St.Label
                                 ({ text: _("Open with %s").format(app.get_display_name()) })
                               });
        box.add(label);

        let button = new St.Button({ child: box,
                                     button_mask: St.ButtonMask.ONE,
                                     style_class: 'hotplug-notification-item' });

        button.connect('clicked', Lang.bind(this, function() {
            startAppForMount(app, this._mount);
            this.destroy();
        }));

        return button;
    },

    _buttonForEject: function() {
        let box = new St.BoxLayout();
        let icon = new St.Icon({ icon_name: 'media-eject',
                                 style_class: 'hotplug-notification-item-icon' });
        box.add(icon);

        let label = new St.Bin({ y_align: St.Align.MIDDLE,
                                 child: new St.Label
                                 ({ text: _("Eject") })
                               });
        box.add(label);

        let button = new St.Button({ child: box,
                                     x_fill: true,
                                     x_align: St.Align.START,
                                     button_mask: St.ButtonMask.ONE,
                                     style_class: 'hotplug-notification-item' });

        button.connect('clicked', Lang.bind(this, function() {
            Main.autorunManager.ejectMount(this._mount);
        }));

        return button;
    }
}

