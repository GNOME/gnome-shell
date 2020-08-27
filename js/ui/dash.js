// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Dash */

const { Clutter, GLib, GObject,
        Graphene, Meta, Shell, St } = imports.gi;

const AppDisplay = imports.ui.appDisplay;
const AppFavorites = imports.ui.appFavorites;
const DND = imports.ui.dnd;
const IconGrid = imports.ui.iconGrid;
const Main = imports.ui.main;

var DASH_ANIMATION_TIME = 200;
var DASH_ITEM_LABEL_SHOW_TIME = 150;
var DASH_ITEM_LABEL_HIDE_TIME = 100;
var DASH_ITEM_HOVER_TIMEOUT = 300;

function getAppFromSource(source) {
    if (source instanceof AppDisplay.AppIcon)
        return source.app;
    else
        return null;
}

var DashIcon = GObject.registerClass(
class DashIcon extends AppDisplay.AppIcon {
    _init(app) {
        super._init(app, {
            setSizeManually: true,
            showLabel: false,
        });
    }

    // Disable all DnD methods
    _onDragBegin() {
    }

    _onDragEnd() {
    }

    handleDragOver() {
        return DND.DragMotionResult.CONTINUE;
    }

    acceptDrop() {
        return false;
    }
});

// A container like StBin, but taking the child's scale into account
// when requesting a size
var DashItemContainer = GObject.registerClass(
class DashItemContainer extends St.Widget {
    _init() {
        super._init({ style_class: 'dash-item-container',
                      pivot_point: new Graphene.Point({ x: .5, y: .5 }),
                      scale_x: 0,
                      scale_y: 0,
                      opacity: 0,
                      x_expand: true,
                      x_align: Clutter.ActorAlign.CENTER });

        this._labelText = "";
        this.label = new St.Label({ style_class: 'dash-label' });
        this.label.hide();
        Main.layoutManager.addChrome(this.label);
        this.label_actor = this.label;

        this.child = null;
        this.animatingOut = false;

        this.connect('notify::scale-x', () => this.queue_relayout());
        this.connect('notify::scale-y', () => this.queue_relayout());

        this.connect('destroy', () => {
            if (this.child != null)
                this.child.destroy();
            this.label.destroy();
        });
    }

    vfunc_get_preferred_height(forWidth) {
        let themeNode = this.get_theme_node();
        forWidth = themeNode.adjust_for_width(forWidth);
        let [minHeight, natHeight] = super.vfunc_get_preferred_height(forWidth);
        return themeNode.adjust_preferred_height(minHeight * this.scale_y,
                                                 natHeight * this.scale_y);
    }

    vfunc_get_preferred_width(forHeight) {
        let themeNode = this.get_theme_node();
        forHeight = themeNode.adjust_for_height(forHeight);
        let [minWidth, natWidth] = super.vfunc_get_preferred_width(forHeight);
        return themeNode.adjust_preferred_width(minWidth * this.scale_x,
                                                natWidth * this.scale_x);
    }

    showLabel() {
        if (!this._labelText)
            return;

        this.label.set_text(this._labelText);
        this.label.opacity = 0;
        this.label.show();

        let [stageX, stageY] = this.get_transformed_position();

        let itemHeight = this.allocation.y2 - this.allocation.y1;

        let labelHeight = this.label.get_height();
        let yOffset = Math.floor((itemHeight - labelHeight) / 2);

        let y = stageY + yOffset;

        let node = this.label.get_theme_node();
        let xOffset = node.get_length('-x-offset');

        let x;
        if (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL)
            x = stageX - this.label.get_width() - xOffset;
        else
            x = stageX + this.get_width() + xOffset;

        this.label.set_position(x, y);
        this.label.ease({
            opacity: 255,
            duration: DASH_ITEM_LABEL_SHOW_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }

    setLabelText(text) {
        this._labelText = text;
        this.child.accessible_name = text;
    }

    hideLabel() {
        this.label.ease({
            opacity: 0,
            duration: DASH_ITEM_LABEL_HIDE_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => this.label.hide(),
        });
    }

    setChild(actor) {
        if (this.child == actor)
            return;

        this.destroy_all_children();

        this.child = actor;
        this.add_actor(this.child);
    }

    show(animate) {
        if (this.child == null)
            return;

        let time = animate ? DASH_ANIMATION_TIME : 0;
        this.ease({
            scale_x: 1,
            scale_y: 1,
            opacity: 255,
            duration: time,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }

    animateOutAndDestroy() {
        this.label.hide();

        if (this.child == null) {
            this.destroy();
            return;
        }

        this.animatingOut = true;
        this.ease({
            scale_x: 0,
            scale_y: 0,
            opacity: 0,
            duration: DASH_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => this.destroy(),
        });
    }
});

var ShowAppsIcon = GObject.registerClass(
class ShowAppsIcon extends DashItemContainer {
    _init() {
        super._init();

        this.toggleButton = new St.Button({ style_class: 'show-apps',
                                            track_hover: true,
                                            can_focus: true,
                                            toggle_mode: true });
        this._iconActor = null;
        this.icon = new IconGrid.BaseIcon(_("Show Applications"),
                                          { setSizeManually: true,
                                            showLabel: false,
                                            createIcon: this._createIcon.bind(this) });
        this.toggleButton.add_actor(this.icon);
        this.toggleButton._delegate = this;

        this.setChild(this.toggleButton);
        this.setDragApp(null);
    }

    _createIcon(size) {
        this._iconActor = new St.Icon({ icon_name: 'view-app-grid-symbolic',
                                        icon_size: size,
                                        style_class: 'show-apps-icon',
                                        track_hover: true });
        return this._iconActor;
    }

    _canRemoveApp(app) {
        if (app == null)
            return false;

        if (!global.settings.is_writable('favorite-apps'))
            return false;

        let id = app.get_id();
        let isFavorite = AppFavorites.getAppFavorites().isFavorite(id);
        return isFavorite;
    }

    setDragApp(app) {
        let canRemove = this._canRemoveApp(app);

        this.toggleButton.set_hover(canRemove);
        if (this._iconActor)
            this._iconActor.set_hover(canRemove);

        if (canRemove)
            this.setLabelText(_("Remove from Favorites"));
        else
            this.setLabelText(_("Show Applications"));
    }

    handleDragOver(source, _actor, _x, _y, _time) {
        if (!this._canRemoveApp(getAppFromSource(source)))
            return DND.DragMotionResult.NO_DROP;

        return DND.DragMotionResult.MOVE_DROP;
    }

    acceptDrop(source, _actor, _x, _y, _time) {
        let app = getAppFromSource(source);
        if (!this._canRemoveApp(app))
            return false;

        let id = app.get_id();

        Meta.later_add(Meta.LaterType.BEFORE_REDRAW, () => {
            AppFavorites.getAppFavorites().removeFavorite(id);
            return false;
        });

        return true;
    }
});

var DragPlaceholderItem = GObject.registerClass(
class DragPlaceholderItem extends DashItemContainer {
    _init() {
        super._init();
        this.setChild(new St.Bin({ style_class: 'placeholder' }));
    }
});

var EmptyDropTargetItem = GObject.registerClass(
class EmptyDropTargetItem extends DashItemContainer {
    _init() {
        super._init();
        this.setChild(new St.Bin({ style_class: 'empty-dash-drop-target' }));
    }
});

var DashActor = GObject.registerClass(
class DashActor extends St.Widget {
    _init() {
        let layout = new Clutter.BoxLayout({ orientation: Clutter.Orientation.VERTICAL });
        super._init({
            name: 'dash',
            layout_manager: layout,
            clip_to_allocation: true,
            y_align: Clutter.ActorAlign.CENTER,
        });
    }

    vfunc_allocate(box, flags) {
        let contentBox = this.get_theme_node().get_content_box(box);
        let availWidth = contentBox.x2 - contentBox.x1;

        this.set_allocation(box, flags);

        let [appIcons, showAppsButton] = this.get_children();
        let [, showAppsNatHeight] = showAppsButton.get_preferred_height(availWidth);

        let childBox = new Clutter.ActorBox();
        childBox.x1 = contentBox.x1;
        childBox.y1 = contentBox.y1;
        childBox.x2 = contentBox.x2;
        childBox.y2 = contentBox.y2 - showAppsNatHeight;
        appIcons.allocate(childBox, flags);

        childBox.y1 = contentBox.y2 - showAppsNatHeight;
        childBox.y2 = contentBox.y2;
        showAppsButton.allocate(childBox, flags);
    }

    vfunc_get_preferred_height(forWidth) {
        // We want to request the natural height of all our children
        // as our natural height, so we chain up to StWidget (which
        // then calls BoxLayout), but we only request the showApps
        // button as the minimum size

        let [, natHeight] = super.vfunc_get_preferred_height(forWidth);

        let themeNode = this.get_theme_node();
        let adjustedForWidth = themeNode.adjust_for_width(forWidth);
        let [, showAppsButton] = this.get_children();
        let [minHeight] = showAppsButton.get_preferred_height(adjustedForWidth);
        [minHeight] = themeNode.adjust_preferred_height(minHeight, natHeight);

        return [minHeight, natHeight];
    }
});

const baseIconSizes = [16, 22, 24, 32, 48, 64];

var Dash = GObject.registerClass({
    Signals: { 'icon-size-changed': {} },
}, class Dash extends St.Bin {
    _init() {
        this._maxHeight = -1;
        this.iconSize = 64;
        this._shownInitially = false;

        this._dragPlaceholder = null;
        this._dragPlaceholderPos = -1;
        this._animatingPlaceholdersCount = 0;
        this._showLabelTimeoutId = 0;
        this._resetHoverTimeoutId = 0;
        this._labelShowing = false;

        this._container = new DashActor();
        this._box = new St.BoxLayout({ vertical: true,
                                       clip_to_allocation: true });
        this._box._delegate = this;
        this._container.add_actor(this._box);
        this._container.set_offscreen_redirect(Clutter.OffscreenRedirect.ALWAYS);

        this._showAppsIcon = new ShowAppsIcon();
        this._showAppsIcon.show(false);
        this._showAppsIcon.icon.setIconSize(this.iconSize);
        this._hookUpLabel(this._showAppsIcon);

        this.showAppsButton = this._showAppsIcon.toggleButton;

        this._container.add_actor(this._showAppsIcon);

        super._init({ child: this._container });
        this.connect('notify::height', () => {
            if (this._maxHeight != this.height)
                this._queueRedisplay();
            this._maxHeight = this.height;
        });

        this._workId = Main.initializeDeferredWork(this._box, this._redisplay.bind(this));

        this._appSystem = Shell.AppSystem.get_default();

        this._appSystem.connect('installed-changed', () => {
            AppFavorites.getAppFavorites().reload();
            this._queueRedisplay();
        });
        AppFavorites.getAppFavorites().connect('changed', this._queueRedisplay.bind(this));
        this._appSystem.connect('app-state-changed', this._queueRedisplay.bind(this));

        Main.overview.connect('item-drag-begin',
                              this._onDragBegin.bind(this));
        Main.overview.connect('item-drag-end',
                              this._onDragEnd.bind(this));
        Main.overview.connect('item-drag-cancelled',
                              this._onDragCancelled.bind(this));

        // Translators: this is the name of the dock/favorites area on
        // the left of the overview
        Main.ctrlAltTabManager.addGroup(this, _("Dash"), 'user-bookmarks-symbolic');
    }

    _onDragBegin() {
        this._dragCancelled = false;
        this._dragMonitor = {
            dragMotion: this._onDragMotion.bind(this),
        };
        DND.addDragMonitor(this._dragMonitor);

        if (this._box.get_n_children() == 0) {
            this._emptyDropTarget = new EmptyDropTargetItem();
            this._box.insert_child_at_index(this._emptyDropTarget, 0);
            this._emptyDropTarget.show(true);
        }
    }

    _onDragCancelled() {
        this._dragCancelled = true;
        this._endDrag();
    }

    _onDragEnd() {
        if (this._dragCancelled)
            return;

        this._endDrag();
    }

    _endDrag() {
        this._clearDragPlaceholder();
        this._clearEmptyDropTarget();
        this._showAppsIcon.setDragApp(null);
        DND.removeDragMonitor(this._dragMonitor);
    }

    _onDragMotion(dragEvent) {
        let app = getAppFromSource(dragEvent.source);
        if (app == null)
            return DND.DragMotionResult.CONTINUE;

        let showAppsHovered =
                this._showAppsIcon.contains(dragEvent.targetActor);

        if (!this._box.contains(dragEvent.targetActor) || showAppsHovered)
            this._clearDragPlaceholder();

        if (showAppsHovered)
            this._showAppsIcon.setDragApp(app);
        else
            this._showAppsIcon.setDragApp(null);

        return DND.DragMotionResult.CONTINUE;
    }

    _appIdListToHash(apps) {
        let ids = {};
        for (let i = 0; i < apps.length; i++)
            ids[apps[i].get_id()] = apps[i];
        return ids;
    }

    _queueRedisplay() {
        Main.queueDeferredWork(this._workId);
    }

    _hookUpLabel(item, appIcon) {
        item.child.connect('notify::hover', () => {
            this._syncLabel(item, appIcon);
        });

        item.child.connect('clicked', () => {
            this._labelShowing = false;
            item.hideLabel();
        });

        let id = Main.overview.connect('hiding', () => {
            this._labelShowing = false;
            item.hideLabel();
        });
        item.child.connect('destroy', () => {
            Main.overview.disconnect(id);
        });

        if (appIcon) {
            appIcon.connect('sync-tooltip', () => {
                this._syncLabel(item, appIcon);
            });
        }
    }

    _createAppItem(app) {
        let appIcon = new DashIcon(app);

        appIcon.connect('menu-state-changed',
                        (o, opened) => {
                            this._itemMenuStateChanged(item, opened);
                        });

        let item = new DashItemContainer();
        item.setChild(appIcon);

        // Override default AppIcon label_actor, now the
        // accessible_name is set at DashItemContainer.setLabelText
        appIcon.label_actor = null;
        item.setLabelText(app.get_name());

        appIcon.icon.setIconSize(this.iconSize);
        this._hookUpLabel(item, appIcon);

        return item;
    }

    _itemMenuStateChanged(item, opened) {
        // When the menu closes, it calls sync_hover, which means
        // that the notify::hover handler does everything we need to.
        if (opened) {
            if (this._showLabelTimeoutId > 0) {
                GLib.source_remove(this._showLabelTimeoutId);
                this._showLabelTimeoutId = 0;
            }

            item.hideLabel();
        }
    }

    _syncLabel(item, appIcon) {
        let shouldShow = appIcon ? appIcon.shouldShowTooltip() : item.child.get_hover();

        if (shouldShow) {
            if (this._showLabelTimeoutId == 0) {
                let timeout = this._labelShowing ? 0 : DASH_ITEM_HOVER_TIMEOUT;
                this._showLabelTimeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, timeout,
                    () => {
                        this._labelShowing = true;
                        item.showLabel();
                        this._showLabelTimeoutId = 0;
                        return GLib.SOURCE_REMOVE;
                    });
                GLib.Source.set_name_by_id(this._showLabelTimeoutId, '[gnome-shell] item.showLabel');
                if (this._resetHoverTimeoutId > 0) {
                    GLib.source_remove(this._resetHoverTimeoutId);
                    this._resetHoverTimeoutId = 0;
                }
            }
        } else {
            if (this._showLabelTimeoutId > 0)
                GLib.source_remove(this._showLabelTimeoutId);
            this._showLabelTimeoutId = 0;
            item.hideLabel();
            if (this._labelShowing) {
                this._resetHoverTimeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, DASH_ITEM_HOVER_TIMEOUT,
                    () => {
                        this._labelShowing = false;
                        this._resetHoverTimeoutId = 0;
                        return GLib.SOURCE_REMOVE;
                    });
                GLib.Source.set_name_by_id(this._resetHoverTimeoutId, '[gnome-shell] this._labelShowing');
            }
        }
    }

    _adjustIconSize() {
        // For the icon size, we only consider children which are "proper"
        // icons (i.e. ignoring drag placeholders) and which are not
        // animating out (which means they will be destroyed at the end of
        // the animation)
        let iconChildren = this._box.get_children().filter(actor => {
            return actor.child &&
                   actor.child._delegate &&
                   actor.child._delegate.icon &&
                   !actor.animatingOut;
        });

        iconChildren.push(this._showAppsIcon);

        if (this._maxHeight == -1)
            return;

        let themeNode = this._container.get_theme_node();
        let maxAllocation = new Clutter.ActorBox({ x1: 0, y1: 0,
                                                   x2: 42 /* whatever */,
                                                   y2: this._maxHeight });
        let maxContent = themeNode.get_content_box(maxAllocation);
        let availHeight = maxContent.y2 - maxContent.y1;
        let spacing = themeNode.get_length('spacing');

        let firstButton = iconChildren[0].child;
        let firstIcon = firstButton._delegate.icon;

        // Enforce valid spacings during the size request
        firstIcon.icon.ensure_style();
        let [, iconHeight] = firstIcon.icon.get_preferred_height(-1);
        let [, buttonHeight] = firstButton.get_preferred_height(-1);

        // Subtract icon padding and box spacing from the available height
        availHeight -= iconChildren.length * (buttonHeight - iconHeight) +
                       (iconChildren.length - 1) * spacing;

        let availSize = availHeight / iconChildren.length;

        let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        let iconSizes = baseIconSizes.map(s => s * scaleFactor);

        let newIconSize = baseIconSizes[0];
        for (let i = 0; i < iconSizes.length; i++) {
            if (iconSizes[i] < availSize)
                newIconSize = baseIconSizes[i];
        }

        if (newIconSize == this.iconSize)
            return;

        let oldIconSize = this.iconSize;
        this.iconSize = newIconSize;
        this.emit('icon-size-changed');

        let scale = oldIconSize / newIconSize;
        for (let i = 0; i < iconChildren.length; i++) {
            let icon = iconChildren[i].child._delegate.icon;

            // Set the new size immediately, to keep the icons' sizes
            // in sync with this.iconSize
            icon.setIconSize(this.iconSize);

            // Don't animate the icon size change when the overview
            // is transitioning, not visible or when initially filling
            // the dash
            if (!Main.overview.visible || Main.overview.animationInProgress ||
                !this._shownInitially)
                continue;

            let [targetWidth, targetHeight] = icon.icon.get_size();

            // Scale the icon's texture to the previous size and
            // tween to the new size
            icon.icon.set_size(icon.icon.width * scale,
                               icon.icon.height * scale);

            icon.icon.ease({
                width: targetWidth,
                height: targetHeight,
                duration: DASH_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });
        }
    }

    _redisplay() {
        let favorites = AppFavorites.getAppFavorites().getFavoriteMap();

        let running = this._appSystem.get_running();

        let children = this._box.get_children().filter(actor => {
            return actor.child &&
                   actor.child._delegate &&
                   actor.child._delegate.app;
        });
        // Apps currently in the dash
        let oldApps = children.map(actor => actor.child._delegate.app);
        // Apps supposed to be in the dash
        let newApps = [];

        for (let id in favorites)
            newApps.push(favorites[id]);

        for (let i = 0; i < running.length; i++) {
            let app = running[i];
            if (app.get_id() in favorites)
                continue;
            newApps.push(app);
        }

        // Figure out the actual changes to the list of items; we iterate
        // over both the list of items currently in the dash and the list
        // of items expected there, and collect additions and removals.
        // Moves are both an addition and a removal, where the order of
        // the operations depends on whether we encounter the position
        // where the item has been added first or the one from where it
        // was removed.
        // There is an assumption that only one item is moved at a given
        // time; when moving several items at once, everything will still
        // end up at the right position, but there might be additional
        // additions/removals (e.g. it might remove all the launchers
        // and add them back in the new order even if a smaller set of
        // additions and removals is possible).
        // If above assumptions turns out to be a problem, we might need
        // to use a more sophisticated algorithm, e.g. Longest Common
        // Subsequence as used by diff.
        let addedItems = [];
        let removedActors = [];

        let newIndex = 0;
        let oldIndex = 0;
        while (newIndex < newApps.length || oldIndex < oldApps.length) {
            let oldApp = oldApps.length > oldIndex ? oldApps[oldIndex] : null;
            let newApp = newApps.length > newIndex ? newApps[newIndex] : null;

            // No change at oldIndex/newIndex
            if (oldApp == newApp) {
                oldIndex++;
                newIndex++;
                continue;
            }

            // App removed at oldIndex
            if (oldApp && !newApps.includes(oldApp)) {
                removedActors.push(children[oldIndex]);
                oldIndex++;
                continue;
            }

            // App added at newIndex
            if (newApp && !oldApps.includes(newApp)) {
                addedItems.push({ app: newApp,
                                  item: this._createAppItem(newApp),
                                  pos: newIndex });
                newIndex++;
                continue;
            }

            // App moved
            let nextApp = newApps.length > newIndex + 1
                ? newApps[newIndex + 1] : null;
            let insertHere = nextApp && nextApp == oldApp;
            let alreadyRemoved = removedActors.reduce((result, actor) => {
                let removedApp = actor.child._delegate.app;
                return result || removedApp == newApp;
            }, false);

            if (insertHere || alreadyRemoved) {
                let newItem = this._createAppItem(newApp);
                addedItems.push({ app: newApp,
                                  item: newItem,
                                  pos: newIndex + removedActors.length });
                newIndex++;
            } else {
                removedActors.push(children[oldIndex]);
                oldIndex++;
            }
        }

        for (let i = 0; i < addedItems.length; i++) {
            this._box.insert_child_at_index(addedItems[i].item,
                                            addedItems[i].pos);
        }

        for (let i = 0; i < removedActors.length; i++) {
            let item = removedActors[i];

            // Don't animate item removal when the overview is transitioning
            // or hidden
            if (Main.overview.visible && !Main.overview.animationInProgress)
                item.animateOutAndDestroy();
            else
                item.destroy();
        }

        this._adjustIconSize();

        // Skip animations on first run when adding the initial set
        // of items, to avoid all items zooming in at once

        let animate = this._shownInitially && Main.overview.visible &&
            !Main.overview.animationInProgress;

        if (!this._shownInitially)
            this._shownInitially = true;

        for (let i = 0; i < addedItems.length; i++)
            addedItems[i].item.show(animate);

        // Workaround for https://bugzilla.gnome.org/show_bug.cgi?id=692744
        // Without it, StBoxLayout may use a stale size cache
        this._box.queue_relayout();
    }

    _clearDragPlaceholder() {
        if (this._dragPlaceholder) {
            this._animatingPlaceholdersCount++;
            this._dragPlaceholder.animateOutAndDestroy();
            this._dragPlaceholder.connect('destroy', () => {
                this._animatingPlaceholdersCount--;
            });
            this._dragPlaceholder = null;
        }
        this._dragPlaceholderPos = -1;
    }

    _clearEmptyDropTarget() {
        if (this._emptyDropTarget) {
            this._emptyDropTarget.animateOutAndDestroy();
            this._emptyDropTarget = null;
        }
    }

    handleDragOver(source, actor, x, y, _time) {
        let app = getAppFromSource(source);

        // Don't allow favoriting of transient apps
        if (app == null || app.is_window_backed())
            return DND.DragMotionResult.NO_DROP;

        if (!global.settings.is_writable('favorite-apps'))
            return DND.DragMotionResult.NO_DROP;

        let favorites = AppFavorites.getAppFavorites().getFavorites();
        let numFavorites = favorites.length;

        let favPos = favorites.indexOf(app);

        let children = this._box.get_children();
        let numChildren = children.length;
        let boxHeight = this._box.height;

        // Keep the placeholder out of the index calculation; assuming that
        // the remove target has the same size as "normal" items, we don't
        // need to do the same adjustment there.
        if (this._dragPlaceholder) {
            boxHeight -= this._dragPlaceholder.height;
            numChildren--;
        }

        let pos;
        if (!this._emptyDropTarget)
            pos = Math.floor(y * numChildren / boxHeight);
        else
            pos = 0; // always insert at the top when dash is empty

        if (pos != this._dragPlaceholderPos && pos <= numFavorites && this._animatingPlaceholdersCount == 0) {
            this._dragPlaceholderPos = pos;

            // Don't allow positioning before or after self
            if (favPos != -1 && (pos == favPos || pos == favPos + 1)) {
                this._clearDragPlaceholder();
                return DND.DragMotionResult.CONTINUE;
            }

            // If the placeholder already exists, we just move
            // it, but if we are adding it, expand its size in
            // an animation
            let fadeIn;
            if (this._dragPlaceholder) {
                this._dragPlaceholder.destroy();
                fadeIn = false;
            } else {
                fadeIn = true;
            }

            this._dragPlaceholder = new DragPlaceholderItem();
            this._dragPlaceholder.child.set_width(this.iconSize);
            this._dragPlaceholder.child.set_height(this.iconSize / 2);
            this._box.insert_child_at_index(this._dragPlaceholder,
                                            this._dragPlaceholderPos);
            this._dragPlaceholder.show(fadeIn);
        }

        // Remove the drag placeholder if we are not in the
        // "favorites zone"
        if (pos > numFavorites)
            this._clearDragPlaceholder();

        if (!this._dragPlaceholder)
            return DND.DragMotionResult.NO_DROP;

        let srcIsFavorite = favPos != -1;

        if (srcIsFavorite)
            return DND.DragMotionResult.MOVE_DROP;

        return DND.DragMotionResult.COPY_DROP;
    }

    // Draggable target interface
    acceptDrop(source, _actor, _x, _y, _time) {
        let app = getAppFromSource(source);

        // Don't allow favoriting of transient apps
        if (app == null || app.is_window_backed())
            return false;

        if (!global.settings.is_writable('favorite-apps'))
            return false;

        let id = app.get_id();

        let favorites = AppFavorites.getAppFavorites().getFavoriteMap();

        let srcIsFavorite = id in favorites;

        let favPos = 0;
        let children = this._box.get_children();
        for (let i = 0; i < this._dragPlaceholderPos; i++) {
            if (this._dragPlaceholder &&
                children[i] == this._dragPlaceholder)
                continue;

            let childId = children[i].child._delegate.app.get_id();
            if (childId == id)
                continue;
            if (childId in favorites)
                favPos++;
        }

        // No drag placeholder means we don't want to favorite the app
        // and we are dragging it to its original position
        if (!this._dragPlaceholder)
            return true;

        Meta.later_add(Meta.LaterType.BEFORE_REDRAW, () => {
            let appFavorites = AppFavorites.getAppFavorites();
            if (srcIsFavorite)
                appFavorites.moveFavoriteToPos(id, favPos);
            else
                appFavorites.addFavoriteAtPos(id, favPos);
            return false;
        });

        return true;
    }
});
