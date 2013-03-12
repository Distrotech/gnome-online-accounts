/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2011, 2013 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: David Zeuthen <davidz@redhat.com>
 *          Debarshi Ray <debarshir@gnome.org>
 */

#include "config.h"
#include <glib/gi18n-lib.h>
#include <stdlib.h>

#include "goaimapauthlogin.h"
#include "goaprovider.h"
#include "goautils.h"

/**
 * SECTION:goaimapauthlogin
 * @title: GoaImapAuthLogin
 * @short_description: LOGIN authentication method for IMAP
 *
 * #GoaImapAuthLogin implements the standard <ulink
 * url="http://tools.ietf.org/html/rfc3501#section-6.2.3">LOGIN</ulink>
 * authentication method (e.g. using usernames / passwords) for IMAP.
 */

/**
 * GoaImapAuthLogin:
 *
 * The #GoaImapAuthLogin structure contains only private data
 * and should only be accessed using the provided API.
 */
struct _GoaImapAuthLogin
{
  GoaMailAuth parent_instance;

  GoaProvider *provider;
  GoaObject *object;
  gchar *username;
  gchar *password;
};

typedef struct
{
  GoaMailAuthClass parent_class;

} GoaImapAuthLoginClass;

enum
{
  PROP_0,
  PROP_PROVIDER,
  PROP_OBJECT,
  PROP_USERNAME,
  PROP_PASSWORD
};

static gboolean goa_imap_auth_login_is_needed (GoaMailAuth        *auth);
static gboolean goa_imap_auth_login_run_sync (GoaMailAuth         *_auth,
                                              GCancellable        *cancellable,
                                              GError             **error);
static gboolean goa_imap_auth_login_starttls_sync (GoaMailAuth    *_auth,
                                                   GCancellable   *cancellable,
                                                   GError        **error);

G_DEFINE_TYPE (GoaImapAuthLogin, goa_imap_auth_login, GOA_TYPE_MAIL_AUTH);

/* ---------------------------------------------------------------------------------------------------- */

static void
goa_imap_auth_login_finalize (GObject *object)
{
  GoaImapAuthLogin *auth = GOA_IMAP_AUTH_LOGIN (object);

  g_clear_object (&auth->provider);
  g_clear_object (&auth->object);
  g_free (auth->username);
  g_free (auth->password);

  G_OBJECT_CLASS (goa_imap_auth_login_parent_class)->finalize (object);
}

static void
goa_imap_auth_login_get_property (GObject      *object,
                                  guint         prop_id,
                                  GValue       *value,
                                  GParamSpec   *pspec)
{
  GoaImapAuthLogin *auth = GOA_IMAP_AUTH_LOGIN (object);

  switch (prop_id)
    {
    case PROP_PROVIDER:
      g_value_set_object (value, auth->provider);
      break;

    case PROP_OBJECT:
      g_value_set_object (value, auth->object);
      break;

    case PROP_USERNAME:
      g_value_set_string (value, auth->username);
      break;

    case PROP_PASSWORD:
      g_value_set_string (value, auth->password);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
goa_imap_auth_login_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GoaImapAuthLogin *auth = GOA_IMAP_AUTH_LOGIN (object);

  switch (prop_id)
    {
    case PROP_PROVIDER:
      auth->provider = g_value_dup_object (value);
      break;

    case PROP_OBJECT:
      auth->object = g_value_dup_object (value);
      break;

    case PROP_USERNAME:
      auth->username = g_value_dup_string (value);
      break;

    case PROP_PASSWORD:
      auth->password = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* ---------------------------------------------------------------------------------------------------- */


static void
goa_imap_auth_login_init (GoaImapAuthLogin *client)
{
}

static void
goa_imap_auth_login_class_init (GoaImapAuthLoginClass *klass)
{
  GObjectClass *gobject_class;
  GoaMailAuthClass *auth_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize     = goa_imap_auth_login_finalize;
  gobject_class->get_property = goa_imap_auth_login_get_property;
  gobject_class->set_property = goa_imap_auth_login_set_property;

  auth_class = GOA_MAIL_AUTH_CLASS (klass);
  auth_class->is_needed = goa_imap_auth_login_is_needed;
  auth_class->run_sync = goa_imap_auth_login_run_sync;
  auth_class->starttls_sync = goa_imap_auth_login_starttls_sync;

  /**
   * GoaImapAuthLogin:provider:
   *
   * The #GoaProvider object for the account or %NULL.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PROVIDER,
                                   g_param_spec_object ("provider",
                                                        "provider",
                                                        "provider",
                                                        GOA_TYPE_PROVIDER,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GoaImapAuthLogin:object:
   *
   * The #GoaObject object for the account.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_OBJECT,
                                   g_param_spec_object ("object",
                                                        "object",
                                                        "object",
                                                        GOA_TYPE_OBJECT,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GoaImapAuthLogin:user-name:
   *
   * The user name.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_USERNAME,
                                   g_param_spec_string ("user-name",
                                                        "user-name",
                                                        "user-name",
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GoaImapAuthLogin:password:
   *
   * The password or %NULL.
   *
   * If this is %NULL, the credentials are looked up using
   * goa_utils_lookup_credentials_sync() using the
   * #GoaImapAuthLogin:provider and #GoaImapAuthLogin:object for
   * @provider and @object. The credentials are expected to be a
   * %G_VARIANT_VARDICT and the key <literal>imap-password</literal>
   * is used to look up the password.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PASSWORD,
                                   g_param_spec_string ("password",
                                                        "password",
                                                        "password",
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * goa_imap_auth_login_new:
 * @provider: (allow-none): A #GoaLoginProvider or %NULL.
 * @object: (allow-none): An account object or %NULL.
 * @username: The user name to use.
 * @password: (allow-none): The password to use or %NULL to look it up
 * (see the #GoaImapAuthLogin:password property).
 *
 * Creates a new #GoaMailAuth to be used for username/password
 * authentication using LOGIN over IMAP.
 *
 * Returns: (type GoaImapAuthLogin): A #GoaImapAuthLogin. Free with
 * g_object_unref().
 */
GoaMailAuth *
goa_imap_auth_login_new (GoaProvider       *provider,
                         GoaObject         *object,
                         const gchar       *username,
                         const gchar       *password)
{
  g_return_val_if_fail (provider == NULL || GOA_IS_PROVIDER (provider), NULL);
  g_return_val_if_fail (object == NULL || GOA_IS_OBJECT (object), NULL);
  g_return_val_if_fail (username != NULL, NULL);
  return GOA_MAIL_AUTH (g_object_new (GOA_TYPE_IMAP_AUTH_LOGIN,
                                      "provider", provider,
                                      "object", object,
                                      "user-name", username,
                                      "password", password,
                                      NULL));
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
goa_imap_auth_login_is_needed (GoaMailAuth *_auth)
{
  /* TODO: support authentication-less IMAP servers */
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
goa_imap_auth_login_run_sync (GoaMailAuth         *_auth,
                              GCancellable        *cancellable,
                              GError             **error)
{
  GoaImapAuthLogin *auth = GOA_IMAP_AUTH_LOGIN (_auth);
  GDataInputStream *input;
  GDataOutputStream *output;
  gchar *request;
  gchar *response;
  gboolean ret;
  gchar *password;

  request = NULL;
  response = NULL;
  password = NULL;
  ret = FALSE;

  if (auth->password != NULL)
    {
      password = g_strdup (auth->password);
    }
  else if (auth->provider != NULL && auth->object != NULL)
    {
      GVariant *credentials;
      credentials = goa_utils_lookup_credentials_sync (auth->provider,
                                                       auth->object,
                                                       cancellable,
                                                       error);
      if (credentials == NULL)
        {
          g_prefix_error (error, "Error looking up credentials for IMAP LOGIN in keyring: ");
          goto out;
        }
      if (!g_variant_lookup (credentials, "imap-password", "s", &password))
        {
          g_set_error (error,
                       GOA_ERROR,
                       GOA_ERROR_FAILED,
                       "Did not find imap-password in credentials");
          g_variant_unref (credentials);
          goto out;
        }
      g_variant_unref (credentials);
    }
  else
    {
      g_set_error (error,
                   GOA_ERROR,
                   GOA_ERROR_FAILED,
                   "Cannot do IMAP LOGIN without a password");
      goto out;
    }

  input = goa_mail_auth_get_input (_auth);
  output = goa_mail_auth_get_output (_auth);

  request = g_strdup_printf ("A001 LOGIN \"%s\" \"%s\"\r\n", auth->username, password);
  g_debug ("> A001 LOGIN \"********************\" \"********************\"", request);
  if (!g_data_output_stream_put_string (output, request, cancellable, error))
    goto out;

 again:
  response = g_data_input_stream_read_line (input, NULL, cancellable, error);
  if (response == NULL)
    goto out;
  g_debug ("< %s", response);
  /* ignore untagged responses */
  if (g_str_has_prefix (response, "*"))
    {
      g_free (response);
      goto again;
    }
  if (!g_str_has_prefix (response, "A001 OK"))
    {
      g_set_error (error,
                   GOA_ERROR,
                   GOA_ERROR_FAILED,
                   _("Authentication failed"),
                   response);
      goto out;
    }

  ret = TRUE;

 out:
  g_free (response);
  g_free (request);
  g_free (password);
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
goa_imap_auth_login_starttls_sync (GoaMailAuth         *_auth,
                                   GCancellable        *cancellable,
                                   GError             **error)
{
  GDataInputStream *input;
  GDataOutputStream *output;
  gchar *request;
  gchar *response;
  gboolean ret;

  request = NULL;
  response = NULL;

  ret = FALSE;

  input = goa_mail_auth_get_input (_auth);
  output = goa_mail_auth_get_output (_auth);

  request = g_strdup ("A001 STARTTLS\r\n");
  g_debug ("> %s", request);
  if (!g_data_output_stream_put_string (output, request, cancellable, error))
    goto out;

 again:
  response = g_data_input_stream_read_line (input, NULL, cancellable, error);
  if (response == NULL)
    goto out;
  g_debug ("< %s", response);
  /* ignore untagged responses */
  if (g_str_has_prefix (response, "*"))
    {
      g_free (response);
      goto again;
    }
  if (!g_str_has_prefix (response, "A001 OK"))
    {
      g_set_error (error,
                   GOA_ERROR,
                   GOA_ERROR_FAILED,
                   "Unexpected response `%s' while doing LOGIN authentication",
                   response);
      goto out;
    }

  ret = TRUE;

 out:
  g_free (response);
  g_free (request);
  return ret;
}
