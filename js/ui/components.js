import * as Main from './main.js';

export class ComponentManager {
    constructor() {
        this._allComponents = {};
        this._enabledComponents = [];

        Main.sessionMode.connect('updated', () => {
            this._sessionModeUpdated().catch(logError);
        });

        this._sessionModeUpdated().catch(logError);
    }

    async _sessionModeUpdated() {
        const newEnabledComponents = Main.sessionMode.components;

        await Promise.allSettled([...newEnabledComponents
            .filter(name => !this._enabledComponents.includes(name))
            .map(name => this._enableComponent(name))]);

        this._enabledComponents
            .filter(name => !newEnabledComponents.includes(name))
            .forEach(name => this._disableComponent(name));

        this._enabledComponents = newEnabledComponents;
    }

    async _importComponent(name) {
        const module = await import(`./components/${name}.js`);
        return module.Component;
    }

    async _ensureComponent(name) {
        let component = this._allComponents[name];
        if (component)
            return component;

        if (Main.sessionMode.isLocked)
            return null;

        const constructor = await this._importComponent(name);
        component = new constructor();
        this._allComponents[name] = component;
        return component;
    }

    async _enableComponent(name) {
        const component = await this._ensureComponent(name);
        if (component)
            component.enable();
    }

    _disableComponent(name) {
        const component = this._allComponents[name];
        if (component == null)
            return;
        component.disable();
    }
}
