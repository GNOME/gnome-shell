// SPDX-FileCopyrightText: 2025 Florian Müllner <fmuellner@gnome.org>
// SPDX-License-Identifier: MIT OR LGPL-2.1-or-later

import {defineConfig} from '@eslint/config-helpers';
import globals from 'globals';
import gnome from 'eslint-config-gnome';

export default defineConfig([
    gnome.configs.recommended,
    gnome.configs.jsdoc,
    {
        ignores: [
            '_build-*',
            'data',
            'docs',
            'man',
            'src',
            'subprojects/gvc',
            'subprojects/jasmine-gjs',
            'subprojects/libshew',
        ],
    }, {
        rules: {
            camelcase: ['error', {
                properties: 'never',
            }],
            'consistent-return': 'error',
            'eqeqeq': ['error', 'smart'],
            'key-spacing': ['error', {
                mode: 'minimum',
                beforeColon: false,
                afterColon: true,
            }],
            'prefer-arrow-callback': 'error',
            'jsdoc/require-param-description': 'off',
            'jsdoc/require-jsdoc': ['error', {
                exemptEmptyFunctions: true,
                publicOnly: {
                    esm: true,
                },
            }],
        },
    }, {
        files: [
            'js/**',
            'tests/shell/**',
        ],
        ignores: [
            'js/portalHelper/*',
            'js/extensions/*',
        ],
        languageOptions: {
            globals: {
                global: 'readonly',
                _: 'readonly',
                C_: 'readonly',
                N_: 'readonly',
                ngettext: 'readonly',
            },
        },
    }, {
        files: ['tests/unit/**'],
        rules: {
            'jsdoc/require-jsdoc': 'off',
        },
        languageOptions: {
            globals: {
                ...globals.jasmine,
            },
        },
    }, {
        files: ['subprojects/extensions-app/js/**'],
        languageOptions: {
            globals: {
                _: 'readonly',
                C_: 'readonly',
                N_: 'readonly',
            },
        },
    }, {
        // doc snippets, disable rules that depend on wider context
        files: ['tmp.*/*.js'],
        rules:  {
            'no-undef': 'off',
            'no-unused-vars': 'off',
            'no-invalid-this': 'off',
        },
    },
]);
