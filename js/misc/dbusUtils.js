// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported loadInterfaceXML, loadSubInterfaceXML */

const Config = imports.misc.config;
const { Gio, GLib } = imports.gi;

let _ifaceResource = null;

/**
 * @private
 */
function _ensureIfaceResource() {
    if (_ifaceResource)
        return;

    // don't use global.datadir so the method is usable from tests/tools
    let dir = GLib.getenv('GNOME_SHELL_DATADIR') || Config.PKGDATADIR;
    let path = `${dir}/gnome-shell-dbus-interfaces.gresource`;
    _ifaceResource = Gio.Resource.load(path);
    _ifaceResource._register();
}

/**
 * @param {string} iface the interface name
 * @returns {string | null} the XML string or null if it is not found
 */
function loadInterfaceXML(iface) {
    _ensureIfaceResource();

    let uri = `resource:///org/gnome/shell/dbus-interfaces/${iface}.xml`;
    let f = Gio.File.new_for_uri(uri);

    try {
        let [ok_, bytes] = f.load_contents(null);
        return new TextDecoder().decode(bytes);
    } catch (e) {
        log(`Failed to load D-Bus interface ${iface}`);
    }

    return null;
}

/**
 * @param {string} iface the interface name
 * @param {string} ifaceFile the interface filename
 * @returns {string | null} the XML string or null if it is not found
 */
function loadSubInterfaceXML(iface, ifaceFile) {
    let xml = loadInterfaceXML(ifaceFile);
    if (!xml)
        return null;

    let ifaceStartTag = `<interface name="${iface}">`;
    let ifaceStopTag = '</interface>';
    let ifaceStartIndex = xml.indexOf(ifaceStartTag);
    let ifaceEndIndex = xml.indexOf(ifaceStopTag, ifaceStartIndex + 1) + ifaceStopTag.length;

    let xmlHeader = '<!DOCTYPE node PUBLIC\n' +
        '\'-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\'\n' +
        '\'http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\'>\n' +
        '<node>\n';
    let xmlFooter = '</node>';

    return (
        xmlHeader +
        xml.substr(ifaceStartIndex, ifaceEndIndex - ifaceStartIndex) +
        xmlFooter);
}
