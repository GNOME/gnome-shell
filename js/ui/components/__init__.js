
const Lang = imports.lang;
const Main = imports.ui.main;

var ComponentManager = new Lang.Class({
    Name: 'ComponentManager',

    _init() {
        this._allComponents = {};
        this._enabledComponents = [];

        Main.sessionMode.connect('updated', Lang.bind(this, this._sessionUpdated));
        this._sessionUpdated();
    },

    _sessionUpdated() {
        let newEnabledComponents = Main.sessionMode.components;

        newEnabledComponents.filter(Lang.bind(this, function(name) {
            return this._enabledComponents.indexOf(name) == -1;
        })).forEach(Lang.bind(this, function(name) {
            this._enableComponent(name);
        }));

        this._enabledComponents.filter(Lang.bind(this, function(name) {
            return newEnabledComponents.indexOf(name) == -1;
        })).forEach(Lang.bind(this, function(name) {
            this._disableComponent(name);
        }));

        this._enabledComponents = newEnabledComponents;
    },

    _importComponent(name) {
        let module = imports.ui.components[name];
        return module.Component;
    },

    _ensureComponent(name) {
        let component = this._allComponents[name];
        if (component)
            return component;

	if (Main.sessionMode.isLocked)
	    return null;

        let constructor = this._importComponent(name);
        component = new constructor();
        this._allComponents[name] = component;
        return component;
    },

    _enableComponent(name) {
        let component = this._ensureComponent(name);
	if (component)
            component.enable();
    },

    _disableComponent(name) {
        let component = this._allComponents[name];
        if (component == null)
            return;
        component.disable();
    }
});
