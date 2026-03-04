/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2025 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>

#include <glib.h>
#include <json-glib/json-glib.h>
#include <syslog.h>

#include <security/pam_modules.h>
#include <security/pam_ext.h>

#include <gdm/gdm-pam-extensions-common.h>
#include <gdm/gdm-custom-json-pam-extension.h>

#define PROTOCOL_NAME "auth-mechanisms"
#define PROTOCOL_VERSION 1

static char *
generate_code (void)
{
        static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        char *code = g_malloc (10);

        for (int i = 0; i < 4; i++)
                code[i] = chars[g_random_int_range (0, sizeof (chars) - 1)];
        code[4] = '-';
        for (int i = 5; i < 9; i++)
                code[i] = chars[g_random_int_range (0, sizeof (chars) - 1)];
        code[9] = '\0';

        return code;
}

static void
add_certificate (JsonBuilder *builder,
                 const char  *key_id,
                 const char  *token_name,
                 const char  *label,
                 const char  *pin_prompt,
                 const char  *cert_instruction)
{
        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "keyId");
        json_builder_add_string_value (builder, key_id);
        json_builder_set_member_name (builder, "tokenName");
        json_builder_add_string_value (builder, token_name);
        json_builder_set_member_name (builder, "moduleName");
        json_builder_add_string_value (builder, "opensc-pkcs11.so");
        json_builder_set_member_name (builder, "label");
        json_builder_add_string_value (builder, label);
        json_builder_set_member_name (builder, "pinPrompt");
        json_builder_add_string_value (builder, pin_prompt);
        json_builder_set_member_name (builder, "certInstruction");
        json_builder_add_string_value (builder, cert_instruction);
        json_builder_end_object (builder);
}

static char *
get_test_json (void)
{
        g_autoptr(JsonBuilder) builder = NULL;
        g_autoptr(JsonGenerator) generator = NULL;
        g_autoptr(JsonNode) root = NULL;
        g_autofree char *code = NULL;

        code = generate_code ();

        builder = json_builder_new ();

        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "authSelection");
        json_builder_begin_object (builder);

        /* mechanisms */
        json_builder_set_member_name (builder, "mechanisms");
        json_builder_begin_object (builder);

        /* password */
        json_builder_set_member_name (builder, "password");
        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "name");
        json_builder_add_string_value (builder, "Password");
        json_builder_set_member_name (builder, "role");
        json_builder_add_string_value (builder, "password");
        json_builder_set_member_name (builder, "prompt");
        json_builder_add_string_value (builder, "Password:");
        json_builder_end_object (builder);

        /* smartcard */
        json_builder_set_member_name (builder, "smartcard");
        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "name");
        json_builder_add_string_value (builder, "Smartcard");
        json_builder_set_member_name (builder, "role");
        json_builder_add_string_value (builder, "smartcard");
        json_builder_set_member_name (builder, "certificates");
        json_builder_begin_array (builder);
        add_certificate (builder,
                         "test-cert-1",
                         "Test Token 1",
                         "Test Certificate 1",
                         "Enter PIN for Test Certificate 1:",
                         "Test User Certificate\nCN=Test User 1, O=Test Organization, C=US");
        add_certificate (builder,
                         "test-cert-2",
                         "Test Token 2",
                         "Test Certificate 2",
                         "Enter PIN for Test Certificate 2:",
                         "Test Admin Certificate\nCN=Test Admin, O=ACME Corp, C=US");
        json_builder_end_array (builder);
        json_builder_end_object (builder);

        /* passkey */
        json_builder_set_member_name (builder, "passkey");
        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "name");
        json_builder_add_string_value (builder, "Passkey");
        json_builder_set_member_name (builder, "role");
        json_builder_add_string_value (builder, "passkey");
        json_builder_set_member_name (builder, "keyConnected");
        json_builder_add_boolean_value (builder, TRUE);
        json_builder_set_member_name (builder, "initInstruction");
        json_builder_add_string_value (builder, "Insert your passkey device");
        json_builder_set_member_name (builder, "touchInstruction");
        json_builder_add_string_value (builder, "Touch your passkey device");
        json_builder_set_member_name (builder, "pinPrompt");
        json_builder_add_string_value (builder, "Enter passkey PIN:");
        json_builder_set_member_name (builder, "pinAttempts");
        json_builder_add_int_value (builder, 8);
        json_builder_set_member_name (builder, "kerberos");
        json_builder_add_boolean_value (builder, TRUE);
        json_builder_set_member_name (builder, "cryptoChallenge");
        json_builder_add_string_value (builder, "test-challenge-data");
        json_builder_end_object (builder);

        /* eidp */
        json_builder_set_member_name (builder, "eidp");
        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "name");
        json_builder_add_string_value (builder, "Web Login");
        json_builder_set_member_name (builder, "role");
        json_builder_add_string_value (builder, "eidp");
        json_builder_set_member_name (builder, "initPrompt");
        json_builder_add_string_value (builder, "Log In");
        json_builder_set_member_name (builder, "linkPrompt");
        json_builder_add_string_value (builder, "Go to the following link to sign in:");
        json_builder_set_member_name (builder, "uri");
        json_builder_add_string_value (builder, "https://github.com/login/device");
        json_builder_set_member_name (builder, "code");
        json_builder_add_string_value (builder, code);
        json_builder_set_member_name (builder, "timeout");
        json_builder_add_int_value (builder, 300);
        json_builder_end_object (builder);

        json_builder_end_object (builder); /* mechanisms */

        /* priority */
        json_builder_set_member_name (builder, "priority");
        json_builder_begin_array (builder);
        json_builder_add_string_value (builder, "password");
        json_builder_add_string_value (builder, "smartcard");
        json_builder_add_string_value (builder, "passkey");
        json_builder_add_string_value (builder, "eidp");
        json_builder_end_array (builder);

        json_builder_end_object (builder); /* authSelection */
        json_builder_end_object (builder);

        root = json_builder_get_root (builder);
        generator = json_generator_new ();
        json_generator_set_root (generator, root);

        return json_generator_to_data (generator, NULL);
}

static int
send_json_message (pam_handle_t *pamh,
                   const char   *json)
{
        struct pam_conv *conv;
        struct pam_message msg;
        const struct pam_message *msgp = &msg;
        struct pam_response *resp = NULL;
        GdmPamExtensionJSONProtocol request;
        int ret;

        if (!GDM_PAM_EXTENSION_SUPPORTED (GDM_PAM_EXTENSION_CUSTOM_JSON))
                return PAM_IGNORE;

        ret = pam_get_item (pamh, PAM_CONV, (const void **) &conv);
        if (ret != PAM_SUCCESS || conv == NULL || conv->conv == NULL)
                return PAM_CONV_ERR;

        GDM_PAM_EXTENSION_CUSTOM_JSON_REQUEST_INIT (&request,
                                                    PROTOCOL_NAME,
                                                    PROTOCOL_VERSION,
                                                    json);

        GDM_PAM_EXTENSION_MESSAGE_TO_BINARY_PROMPT_MESSAGE (&request, &msg);

        ret = conv->conv (1, &msgp, &resp, conv->appdata_ptr);

        if (resp != NULL) {
                pam_syslog(pamh, LOG_ERR, "Received JSON response: %s", resp->resp);
                g_free (resp->resp);
                g_free (resp);
        }

        return ret == PAM_SUCCESS ? PAM_SUCCESS : PAM_AUTH_ERR;
}

int
pam_sm_authenticate (pam_handle_t  *pamh,
                     int            flags,
                     int            argc,
                     const char   **argv)
{
        g_autofree char *json = NULL;

        json = get_test_json ();

        return send_json_message (pamh, json);
}

int
pam_sm_setcred (pam_handle_t  *pamh,
                int            flags,
                int            argc,
                const char   **argv)
{
        return PAM_SUCCESS;
}

int
pam_sm_acct_mgmt (pam_handle_t  *pamh,
                  int            flags,
                  int            argc,
                  const char   **argv)
{
        return PAM_SUCCESS;
}

int
pam_sm_open_session (pam_handle_t  *pamh,
                     int            flags,
                     int            argc,
                     const char   **argv)
{
        return PAM_SUCCESS;
}

int
pam_sm_close_session (pam_handle_t  *pamh,
                      int            flags,
                      int            argc,
                      const char   **argv)
{
        return PAM_SUCCESS;
}

int
pam_sm_chauthtok (pam_handle_t  *pamh,
                  int            flags,
                  int            argc,
                  const char   **argv)
{
        return PAM_SUCCESS;
}
