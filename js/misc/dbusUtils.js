import Gio from 'gi://Gio';
import GLib from 'gi://GLib';

import * as Config from './config.js';

let _ifaceResource = null;

/**
 * @private
 */
function _ensureIfaceResource() {
    if (_ifaceResource)
        return;

    // don't use global.datadir so the method is usable from tests/tools
    const dir = GLib.getenv('GNOME_SHELL_DATADIR') || Config.PKGDATADIR;
    const path = `${dir}/gnome-shell-dbus-interfaces.gresource`;
    _ifaceResource = Gio.Resource.load(path);
    _ifaceResource._register();
}

/**
 * @param {string} iface the interface name
 * @returns {string | null} the XML string or null if it is not found
 */
export function loadInterfaceXML(iface) {
    _ensureIfaceResource();

    const uri = `resource:///org/gnome/shell/dbus-interfaces/${iface}.xml`;
    const f = Gio.File.new_for_uri(uri);

    try {
        const [ok_, bytes] = f.load_contents(null);
        return new TextDecoder().decode(bytes);
    } catch {
        log(`Failed to load D-Bus interface ${iface}`);
    }

    return null;
}

/**
 * @param {string} iface the interface name
 * @param {string} ifaceFile the interface filename
 * @returns {string | null} the XML string or null if it is not found
 */
export function loadSubInterfaceXML(iface, ifaceFile) {
    const xml = loadInterfaceXML(ifaceFile);
    if (!xml)
        return null;

    const ifaceStartTag = `<interface name="${iface}">`;
    const ifaceStopTag = '</interface>';
    const ifaceStartIndex = xml.indexOf(ifaceStartTag);
    const ifaceEndIndex = xml.indexOf(ifaceStopTag, ifaceStartIndex + 1) + ifaceStopTag.length;

    const xmlHeader = '<!DOCTYPE node PUBLIC\n' +
        '\'-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\'\n' +
        '\'http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\'>\n' +
        '<node>\n';
    const xmlFooter = '</node>';

    return (
        xmlHeader +
        xml.substring(ifaceStartIndex, ifaceEndIndex) +
        xmlFooter);
}
