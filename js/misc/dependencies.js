import gi from 'gi';

/**
 * Required dependencies
 */

import 'gi://AccountsService?version=1.0';
import 'gi://Atk?version=1.0';
import 'gi://Atspi?version=2.0';
import 'gi://Gcr?version=4';
import 'gi://Gdk?version=4.0';
import 'gi://Gdm?version=1.0';
import 'gi://Geoclue?version=2.0';
import 'gi://Gio?version=2.0';
import 'gi://GioUnix?version=2.0';
import 'gi://GDesktopEnums?version=3.0';
import 'gi://GdkPixbuf?version=2.0';
import 'gi://GnomeBG?version=4.0';
import 'gi://GnomeDesktop?version=4.0';
import 'gi://Graphene?version=1.0';
import 'gi://GWeather?version=4.0';
import 'gi://IBus?version=1.0';
import 'gi://Pango?version=1.0';
import 'gi://Polkit?version=1.0';
import 'gi://PolkitAgent?version=1.0';
import 'gi://Rsvg?version=2.0';
import 'gi://Soup?version=3.0';
import 'gi://UPowerGlib?version=1.0';

import * as Config from './config.js';

// Meta-related dependencies use a shared version
// from the compile-time config.
gi.require('Meta', Config.LIBMUTTER_API_VERSION);
gi.require('Clutter', Config.LIBMUTTER_API_VERSION);
gi.require('Cogl', Config.LIBMUTTER_API_VERSION);
gi.require('Shell', Config.LIBMUTTER_API_VERSION);
gi.require('St', Config.LIBMUTTER_API_VERSION);

/**
 * Compile-time optional dependencies
 */

if (Config.HAVE_BLUETOOTH)
    gi.require('GnomeBluetooth', '3.0');
else
    console.debug('GNOME Shell was compiled without GNOME Bluetooth support');


if (Config.HAVE_NETWORKMANAGER) {
    gi.require('NM', '1.0');
    gi.require('NMA4', '1.0');
} else {
    console.debug('GNOME Shell was compiled without Network Manager support');
}

/**
 * Runtime optional dependencies
 */

try {
    // Malcontent is optional, so catch any errors loading it
    gi.require('Malcontent', '0');
} catch {
    console.debug('Malcontent is not available, parental controls integration will be disabled.');
}
