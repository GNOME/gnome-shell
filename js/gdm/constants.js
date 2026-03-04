import GLib from 'gi://GLib';

export const PASSWORD_ROLE_NAME = 'password';
export const SMARTCARD_ROLE_NAME = 'smartcard';
export const FINGERPRINT_ROLE_NAME = 'fingerprint';
export const PASSKEY_ROLE_NAME = 'passkey';
export const WEB_LOGIN_ROLE_NAME = 'eidp';

export const PASSWORD_SERVICE_NAME = 'gdm-password';
export const SMARTCARD_SERVICE_NAME = 'gdm-smartcard';
export const FINGERPRINT_SERVICE_NAME = 'gdm-fingerprint';
export const SWITCHABLE_AUTH_TEST_MODE = GLib.getenv('GDM_SWITCHABLE_AUTH_TEST') !== null;
export const SWITCHABLE_AUTH_SERVICE_NAME = SWITCHABLE_AUTH_TEST_MODE
    ? 'gdm-switchable-auth-test'
    : 'gdm-switchable-auth';
