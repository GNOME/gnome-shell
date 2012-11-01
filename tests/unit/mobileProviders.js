// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const GLib = imports.gi.GLib;
const Shell = imports.gi.Shell;
const JsUnit = imports.jsUnit;
const ModemManager = imports.misc.modemManager;
const Environment = imports.ui.environment;

Environment.init();

// Load test providers table
let countrycodesPath = GLib.getenv("GNOME_SHELL_TESTSDIR") + "/testcommon/iso3166-test.tab";
let serviceprovidersPath = GLib.getenv("GNOME_SHELL_TESTSDIR") + "/testcommon/serviceproviders-test.xml";
let providersTable = Shell.mobile_providers_parse(countrycodesPath, serviceprovidersPath);

function assertCountryFound(country_code, expected_country_name) {
    let country = providersTable[country_code];
    JsUnit.assertNotUndefined(country);
    JsUnit.assertEquals(country.get_country_name(), expected_country_name);
}

function assertCountryNotFound(country_code) {
    let country = providersTable[country_code];
    JsUnit.assertUndefined(country);
}

function assertProviderFoundForMCCMNC(mccmnc, expected_provider_name) {
    let provider_name = ModemManager.findProviderForMCCMNC(providersTable, mccmnc);
    JsUnit.assertEquals(provider_name, expected_provider_name);
}

function assertProviderNotFoundForMCCMNC(mccmnc) {
    let provider_name = ModemManager.findProviderForMCCMNC(providersTable, mccmnc);
    JsUnit.assertNull(provider_name);
}

function assertProviderFoundForSid(sid, expected_provider_name) {
    let provider_name = ModemManager.findProviderForSid(providersTable, sid);
    JsUnit.assertEquals(provider_name, expected_provider_name);
}

function assertProviderNotFoundForSid(sid) {
    let provider_name = ModemManager.findProviderForSid(providersTable, sid);
    JsUnit.assertNull(provider_name);
}

// TEST:
// * Both 'US' and 'ES' country info should be loaded
assertCountryFound("ES", "Spain");
assertCountryFound("US", "United States");

// TEST:
// * Country info for 'FR' not given
assertCountryNotFound("FR");

// TEST:
// * Ensure operator names are found for the given MCC/MNC codes
assertProviderFoundForMCCMNC("21405", "Movistar (Telefónica)");
assertProviderFoundForMCCMNC("21407", "Movistar (Telefónica)");
assertProviderFoundForMCCMNC("310038", "AT&T");
assertProviderFoundForMCCMNC("310090", "AT&T");
assertProviderFoundForMCCMNC("310150", "AT&T");
assertProviderFoundForMCCMNC("310995", "Verizon");
assertProviderFoundForMCCMNC("311480", "Verizon");

// TEST:
// * Ensure NULL is given for unexpected MCC/MNC codes
assertProviderNotFoundForMCCMNC("12345");

// TEST:
// * Ensure operator names are found for the given SID codes
assertProviderFoundForSid(2, "Verizon");
assertProviderFoundForSid(4, "Verizon");
assertProviderFoundForSid(5, "Verizon");

// TEST:
// * Ensure NULL is given for unexpected SID codes
assertProviderNotFoundForSid(1);
