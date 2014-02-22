/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "cogl-context-private.h"
#include "cogl-debug.h"
#include "cogl-blend-string.h"
#include "cogl-error-private.h"

typedef enum _ParserState
{
  PARSER_STATE_EXPECT_DEST_CHANNELS,
  PARSER_STATE_SCRAPING_DEST_CHANNELS,
  PARSER_STATE_EXPECT_FUNCTION_NAME,
  PARSER_STATE_SCRAPING_FUNCTION_NAME,
  PARSER_STATE_EXPECT_ARG_START,
  PARSER_STATE_EXPECT_STATEMENT_END
} ParserState;

typedef enum _ParserArgState
{
  PARSER_ARG_STATE_START,
  PARSER_ARG_STATE_EXPECT_MINUS,
  PARSER_ARG_STATE_EXPECT_COLOR_SRC_NAME,
  PARSER_ARG_STATE_SCRAPING_COLOR_SRC_NAME,
  PARSER_ARG_STATE_MAYBE_COLOR_MASK,
  PARSER_ARG_STATE_SCRAPING_MASK,
  PARSER_ARG_STATE_MAYBE_MULT,
  PARSER_ARG_STATE_EXPECT_OPEN_PAREN,
  PARSER_ARG_STATE_EXPECT_FACTOR,
  PARSER_ARG_STATE_MAYBE_SRC_ALPHA_SATURATE,
  PARSER_ARG_STATE_MAYBE_MINUS,
  PARSER_ARG_STATE_EXPECT_CLOSE_PAREN,
  PARSER_ARG_STATE_EXPECT_END
} ParserArgState;


#define DEFINE_COLOR_SOURCE(NAME, NAME_LEN) \
  {/*.type = */COGL_BLEND_STRING_COLOR_SOURCE_ ## NAME, \
   /*.name = */#NAME, \
   /*.name_len = */NAME_LEN}

static CoglBlendStringColorSourceInfo blending_color_sources[] = {
  DEFINE_COLOR_SOURCE (SRC_COLOR, 9),
  DEFINE_COLOR_SOURCE (DST_COLOR, 9),
  DEFINE_COLOR_SOURCE (CONSTANT, 8)
};

static CoglBlendStringColorSourceInfo tex_combine_color_sources[] = {
  DEFINE_COLOR_SOURCE (TEXTURE, 7),
  /* DEFINE_COLOR_SOURCE (TEXTURE_N, *) - handled manually */
  DEFINE_COLOR_SOURCE (PRIMARY, 7),
  DEFINE_COLOR_SOURCE (CONSTANT, 8),
  DEFINE_COLOR_SOURCE (PREVIOUS, 8)
};

static CoglBlendStringColorSourceInfo tex_combine_texture_n_color_source = {
  /*.type = */COGL_BLEND_STRING_COLOR_SOURCE_TEXTURE_N,
  /*.name = */"TEXTURE_N",
  /*.name_len = */0
};

#undef DEFINE_COLOR_SOURCE

#define DEFINE_FUNCTION(NAME, NAME_LEN, ARGC) \
  { /*.type = */COGL_BLEND_STRING_FUNCTION_ ## NAME, \
    /*.name = */#NAME, \
    /*.name_len = */NAME_LEN, \
    /*.argc = */ARGC }

/* NB: These must be sorted so any name that's a subset of another
 * comes later than the longer name. */
static CoglBlendStringFunctionInfo tex_combine_functions[] = {
  DEFINE_FUNCTION (REPLACE, 7, 1),
  DEFINE_FUNCTION (MODULATE, 8, 2),
  DEFINE_FUNCTION (ADD_SIGNED, 10, 2),
  DEFINE_FUNCTION (ADD, 3, 2),
  DEFINE_FUNCTION (INTERPOLATE, 11, 3),
  DEFINE_FUNCTION (SUBTRACT, 8, 2),
  DEFINE_FUNCTION (DOT3_RGBA, 9, 2),
  DEFINE_FUNCTION (DOT3_RGB, 8, 2)
};

static CoglBlendStringFunctionInfo blend_functions[] = {
  DEFINE_FUNCTION (ADD, 3, 2)
};

#undef DEFINE_FUNCTION

uint32_t
cogl_blend_string_error_quark (void)
{
  return g_quark_from_static_string ("cogl-blend-string-error-quark");
}

void
_cogl_blend_string_split_rgba_statement (CoglBlendStringStatement *statement,
                                         CoglBlendStringStatement *rgb,
                                         CoglBlendStringStatement *a)
{
  int i;

  memcpy (rgb, statement, sizeof (CoglBlendStringStatement));
  memcpy (a, statement, sizeof (CoglBlendStringStatement));

  rgb->mask = COGL_BLEND_STRING_CHANNEL_MASK_RGB;
  a->mask = COGL_BLEND_STRING_CHANNEL_MASK_ALPHA;

  for (i = 0; i < statement->function->argc; i++)
    {
      CoglBlendStringArgument *arg = &statement->args[i];
      CoglBlendStringArgument *rgb_arg = &rgb->args[i];
      CoglBlendStringArgument *a_arg = &a->args[i];

      if (arg->source.mask == COGL_BLEND_STRING_CHANNEL_MASK_RGBA)
        {
          rgb_arg->source.mask = COGL_BLEND_STRING_CHANNEL_MASK_RGB;
          a_arg->source.mask = COGL_BLEND_STRING_CHANNEL_MASK_ALPHA;
        }

      if (arg->factor.is_color &&
          arg->factor.source.mask == COGL_BLEND_STRING_CHANNEL_MASK_RGBA)
        {
          rgb_arg->factor.source.mask = COGL_BLEND_STRING_CHANNEL_MASK_RGB;
          a_arg->factor.source.mask = COGL_BLEND_STRING_CHANNEL_MASK_ALPHA;
        }
    }
}

static CoglBool
validate_tex_combine_statements (CoglBlendStringStatement *statements,
                                 int n_statements,
                                 CoglError **error)
{
  int i, j;
  const char *error_string;
  CoglBlendStringError detail = COGL_BLEND_STRING_ERROR_INVALID_ERROR;

  for (i = 0; i < n_statements; i++)
    {
      for (j = 0; j < statements[i].function->argc; j++)
        {
          CoglBlendStringArgument *arg = &statements[i].args[j];
          if (arg->source.is_zero)
            {
              error_string = "You can't use the constant '0' as a texture "
                             "combine argument";
              goto error;
            }
          if (!arg->factor.is_one)
            {
              error_string = "Argument factors are only relevant to blending "
                             "not texture combining";
              goto error;
            }
        }
    }

  return TRUE;

error:
  _cogl_set_error (error,
                   COGL_BLEND_STRING_ERROR,
                   detail,
                   "Invalid texture combine string: %s",
                   error_string);

  if (COGL_DEBUG_ENABLED (COGL_DEBUG_BLEND_STRINGS))
    {
      g_debug ("Invalid texture combine string: %s",
               error_string);
    }
  return FALSE;
}

static CoglBool
validate_blend_statements (CoglBlendStringStatement *statements,
                           int n_statements,
                           CoglError **error)
{
  int i, j;
  const char *error_string;
  CoglBlendStringError detail = COGL_BLEND_STRING_ERROR_INVALID_ERROR;

  _COGL_GET_CONTEXT (ctx, 0);

  if (n_statements == 2 &&
      !ctx->glBlendEquationSeparate &&
      statements[0].function->type != statements[1].function->type)
    {
      error_string = "Separate blend functions for the RGB an A "
        "channels isn't supported by the driver";
      detail = COGL_BLEND_STRING_ERROR_GPU_UNSUPPORTED_ERROR;
      goto error;
    }

  for (i = 0; i < n_statements; i++)
    for (j = 0; j < statements[i].function->argc; j++)
      {
        CoglBlendStringArgument *arg = &statements[i].args[j];

        if (arg->source.is_zero)
          continue;

        if ((j == 0 &&
             arg->source.info->type !=
             COGL_BLEND_STRING_COLOR_SOURCE_SRC_COLOR)
            || (j == 1 &&
                arg->source.info->type !=
                COGL_BLEND_STRING_COLOR_SOURCE_DST_COLOR))
          {
            error_string = "For blending you must always use SRC_COLOR "
                           "for arg0 and DST_COLOR for arg1";
            goto error;
          }

        if (!_cogl_has_private_feature (ctx,
                                        COGL_PRIVATE_FEATURE_BLEND_CONSTANT) &&
            arg->factor.is_color &&
            (arg->factor.source.info->type ==
             COGL_BLEND_STRING_COLOR_SOURCE_CONSTANT))
          {
            error_string = "Driver doesn't support constant blend factors";
            detail = COGL_BLEND_STRING_ERROR_GPU_UNSUPPORTED_ERROR;
            goto error;
          }
      }

  return TRUE;

error:
  _cogl_set_error (error,
                   COGL_BLEND_STRING_ERROR,
                   detail,
                   "Invalid blend string: %s",
                   error_string);
  return FALSE;
}

static CoglBool
validate_statements_for_context (CoglBlendStringStatement *statements,
                                 int n_statements,
                                 CoglBlendStringContext context,
                                 CoglError **error)
{
  const char *error_string;

  if (n_statements == 1)
    {
      if (statements[0].mask == COGL_BLEND_STRING_CHANNEL_MASK_ALPHA)
        {
          error_string = "You need to also give a blend statement for the RGB"
                         "channels";
          goto error;
        }
      else if (statements[0].mask == COGL_BLEND_STRING_CHANNEL_MASK_RGB)
        {
          error_string = "You need to also give a blend statement for the "
                         "Alpha channel";
          goto error;
        }
    }

  if (context == COGL_BLEND_STRING_CONTEXT_BLENDING)
    return validate_blend_statements (statements, n_statements, error);
  else
    return validate_tex_combine_statements (statements, n_statements, error);

error:
  _cogl_set_error (error,
                   COGL_BLEND_STRING_ERROR,
                   COGL_BLEND_STRING_ERROR_INVALID_ERROR,
                   "Invalid %s string: %s",
                   context == COGL_BLEND_STRING_CONTEXT_BLENDING ?
                   "blend" : "texture combine",
                   error_string);

  if (COGL_DEBUG_ENABLED (COGL_DEBUG_BLEND_STRINGS))
    {
      g_debug ("Invalid %s string: %s",
               context == COGL_BLEND_STRING_CONTEXT_BLENDING ?
               "blend" : "texture combine",
               error_string);
    }

  return FALSE;
}

static void
print_argument (CoglBlendStringArgument *arg)
{
  const char *mask_names[] = {
      "RGB",
      "A",
      "RGBA"
  };

  g_print (" Arg:\n");
  g_print ("  is zero = %s\n", arg->source.is_zero ? "yes" : "no");
  if (!arg->source.is_zero)
    {
      g_print ("  color source = %s\n", arg->source.info->name);
      g_print ("  one minus = %s\n", arg->source.one_minus ? "yes" : "no");
      g_print ("  mask = %s\n", mask_names[arg->source.mask]);
      g_print ("  texture = %d\n", arg->source.texture);
      g_print ("\n");
      g_print ("  factor is_one = %s\n", arg->factor.is_one ? "yes" : "no");
      g_print ("  factor is_src_alpha_saturate = %s\n",
               arg->factor.is_src_alpha_saturate ? "yes" : "no");
      g_print ("  factor is_color = %s\n", arg->factor.is_color ? "yes" : "no");
      if (arg->factor.is_color)
        {
          g_print ("  factor color:is zero = %s\n",
                   arg->factor.source.is_zero ? "yes" : "no");
          g_print ("  factor color:color source = %s\n",
                   arg->factor.source.info->name);
          g_print ("  factor color:one minus = %s\n",
                   arg->factor.source.one_minus ? "yes" : "no");
          g_print ("  factor color:mask = %s\n",
                   mask_names[arg->factor.source.mask]);
          g_print ("  factor color:texture = %d\n",
                   arg->factor.source.texture);
        }
    }
}

static void
print_statement (int num, CoglBlendStringStatement *statement)
{
  const char *mask_names[] = {
      "RGB",
      "A",
      "RGBA"
  };
  int i;
  g_print ("Statement %d:\n", num);
  g_print (" Destination channel mask = %s\n",
           mask_names[statement->mask]);
  g_print (" Function = %s\n", statement->function->name);
  for (i = 0; i < statement->function->argc; i++)
    print_argument (&statement->args[i]);
}

static const CoglBlendStringFunctionInfo *
get_function_info (const char *mark,
                   const char *p,
                   CoglBlendStringContext context)
{
  size_t len = p - mark;
  CoglBlendStringFunctionInfo *functions;
  size_t array_len;
  int i;

  if (context == COGL_BLEND_STRING_CONTEXT_BLENDING)
    {
      functions = blend_functions;
      array_len = G_N_ELEMENTS (blend_functions);
    }
  else
    {
      functions = tex_combine_functions;
      array_len = G_N_ELEMENTS (tex_combine_functions);
    }

  for (i = 0; i < array_len; i++)
    {
      if (len >= functions[i].name_len
          && strncmp (mark, functions[i].name, functions[i].name_len) == 0)
        return &functions[i];
    }
  return NULL;
}

static const CoglBlendStringColorSourceInfo *
get_color_src_info (const char *mark,
                    const char *p,
                    CoglBlendStringContext context)
{
  size_t len = p - mark;
  CoglBlendStringColorSourceInfo *sources;
  size_t array_len;
  int i;

  if (context == COGL_BLEND_STRING_CONTEXT_BLENDING)
    {
      sources = blending_color_sources;
      array_len = G_N_ELEMENTS (blending_color_sources);
    }
  else
    {
      sources = tex_combine_color_sources;
      array_len = G_N_ELEMENTS (tex_combine_color_sources);
    }

  if (len >= 8 &&
      strncmp (mark, "TEXTURE_", 8) == 0 &&
      g_ascii_isdigit (mark[8]))
    {
      return &tex_combine_texture_n_color_source;
    }

  for (i = 0; i < array_len; i++)
    {
      if (len >= sources[i].name_len
          && strncmp (mark, sources[i].name, sources[i].name_len) == 0)
        return &sources[i];
    }

  return NULL;
}

static CoglBool
is_symbol_char (const char c)
{
  return (g_ascii_isalpha (c) || c == '_') ? TRUE : FALSE;
}

static CoglBool
is_alphanum_char (const char c)
{
  return (g_ascii_isalnum (c) || c == '_') ? TRUE : FALSE;
}

static CoglBool
parse_argument (const char *string, /* original user string */
                const char **ret_p, /* start of argument IN:OUT */
                const CoglBlendStringStatement *statement,
                int current_arg,
                CoglBlendStringArgument *arg, /* OUT */
                CoglBlendStringContext context,
                CoglError **error)
{
  const char *p = *ret_p;
  const char *mark = NULL;
  const char *error_string = NULL;
  ParserArgState state = PARSER_ARG_STATE_START;
  CoglBool parsing_factor = FALSE;
  CoglBool implicit_factor_brace;

  arg->source.is_zero = FALSE;
  arg->source.info = NULL;
  arg->source.texture = 0;
  arg->source.one_minus = FALSE;
  arg->source.mask = statement->mask;

  arg->factor.is_one = FALSE;
  arg->factor.is_color = FALSE;
  arg->factor.is_src_alpha_saturate = FALSE;

  arg->factor.source.is_zero = FALSE;
  arg->factor.source.info = NULL;
  arg->factor.source.texture = 0;
  arg->factor.source.one_minus = FALSE;
  arg->factor.source.mask = statement->mask;

  do
    {
      if (g_ascii_isspace (*p))
        continue;

      if (*p == '\0')
        {
          error_string = "Unexpected end of string while parsing argument";
          goto error;
        }

      switch (state)
        {
        case PARSER_ARG_STATE_START:
          if (*p == '1')
            state = PARSER_ARG_STATE_EXPECT_MINUS;
          else if (*p == '0')
            {
              arg->source.is_zero = TRUE;
              state = PARSER_ARG_STATE_EXPECT_END;
            }
          else
            {
              p--; /* backtrack */
              state = PARSER_ARG_STATE_EXPECT_COLOR_SRC_NAME;
            }
          continue;

        case PARSER_ARG_STATE_EXPECT_MINUS:
          if (*p != '-')
            {
              error_string = "expected a '-' following the 1";
              goto error;
            }
          arg->source.one_minus = TRUE;
          state = PARSER_ARG_STATE_EXPECT_COLOR_SRC_NAME;
          continue;

        case PARSER_ARG_STATE_EXPECT_COLOR_SRC_NAME:
          if (!is_symbol_char (*p))
            {
              error_string = "expected a color source name";
              goto error;
            }
          state = PARSER_ARG_STATE_SCRAPING_COLOR_SRC_NAME;
          mark = p;
          if (parsing_factor)
            arg->factor.is_color = TRUE;

          /* fall through */
        case PARSER_ARG_STATE_SCRAPING_COLOR_SRC_NAME:
          if (!is_symbol_char (*p))
            {
              CoglBlendStringColorSource *source =
                parsing_factor ? &arg->factor.source : &arg->source;
              source->info = get_color_src_info (mark, p, context);
              if (!source->info)
                {
                  error_string = "Unknown color source name";
                  goto error;
                }
              if (source->info->type ==
                  COGL_BLEND_STRING_COLOR_SOURCE_TEXTURE_N)
                {
                  char *endp;
                  source->texture =
                    strtoul (&mark[strlen ("TEXTURE_")], &endp, 10);
                  if (mark == endp)
                    {
                      error_string = "invalid texture number given with "
                                     "TEXTURE_N color source";
                      goto error;
                    }
                  p = endp;
                }
              state = PARSER_ARG_STATE_MAYBE_COLOR_MASK;
            }
          else
            continue;

          /* fall through */
        case PARSER_ARG_STATE_MAYBE_COLOR_MASK:
          if (*p != '[')
            {
              p--; /* backtrack */
              if (!parsing_factor)
                state = PARSER_ARG_STATE_MAYBE_MULT;
              else
                state = PARSER_ARG_STATE_EXPECT_END;
              continue;
            }
          state = PARSER_ARG_STATE_SCRAPING_MASK;
          mark = p;

          /* fall through */
        case PARSER_ARG_STATE_SCRAPING_MASK:
          if (*p == ']')
            {
              size_t len = p - mark;
              CoglBlendStringColorSource *source =
                parsing_factor ? &arg->factor.source : &arg->source;

              if (len == 5 && strncmp (mark, "[RGBA", len) == 0)
                {
                  if (statement->mask != COGL_BLEND_STRING_CHANNEL_MASK_RGBA)
                    {
                      error_string = "You can't use an RGBA color mask if the "
                                     "statement hasn't also got an RGBA= mask";
                      goto error;
                    }
                  source->mask = COGL_BLEND_STRING_CHANNEL_MASK_RGBA;
                }
              else if (len == 4 && strncmp (mark, "[RGB", len) == 0)
                source->mask = COGL_BLEND_STRING_CHANNEL_MASK_RGB;
              else if (len == 2 && strncmp (mark, "[A", len) == 0)
                source->mask = COGL_BLEND_STRING_CHANNEL_MASK_ALPHA;
              else
                {
                  error_string = "Expected a channel mask of [RGBA]"
                                 "[RGB] or [A]";
                  goto error;
                }
              if (parsing_factor)
                state = PARSER_ARG_STATE_EXPECT_CLOSE_PAREN;
              else
                state = PARSER_ARG_STATE_MAYBE_MULT;
            }
          continue;

        case PARSER_ARG_STATE_EXPECT_OPEN_PAREN:
          if (*p != '(')
            {
              if (is_alphanum_char (*p))
                {
                  p--; /* compensate for implicit brace and ensure this
                        * char gets considered part of the blend factor */
                  implicit_factor_brace = TRUE;
                }
              else
                {
                  error_string = "Expected '(' around blend factor or alpha "
                                 "numeric character for blend factor name";
                  goto error;
                }
            }
          else
            implicit_factor_brace = FALSE;
          parsing_factor = TRUE;
          state = PARSER_ARG_STATE_EXPECT_FACTOR;
          continue;

        case PARSER_ARG_STATE_EXPECT_FACTOR:
          if (*p == '1')
            state = PARSER_ARG_STATE_MAYBE_MINUS;
          else if (*p == '0')
            {
              arg->source.is_zero = TRUE;
              state = PARSER_ARG_STATE_EXPECT_CLOSE_PAREN;
            }
          else
            {
              state = PARSER_ARG_STATE_MAYBE_SRC_ALPHA_SATURATE;
              mark = p;
            }
          continue;

        case PARSER_ARG_STATE_MAYBE_SRC_ALPHA_SATURATE:
          if (!is_symbol_char (*p))
            {
              size_t len = p - mark;
              if (len >= strlen ("SRC_ALPHA_SATURATE") &&
                  strncmp (mark, "SRC_ALPHA_SATURATE", len) == 0)
                {
                  arg->factor.is_src_alpha_saturate = TRUE;
                  state = PARSER_ARG_STATE_EXPECT_CLOSE_PAREN;
                }
              else
                {
                  state = PARSER_ARG_STATE_EXPECT_COLOR_SRC_NAME;
                  p = mark - 1; /* backtrack */
                }
            }
          continue;

        case PARSER_ARG_STATE_MAYBE_MINUS:
          if (*p == '-')
            {
              if (implicit_factor_brace)
                {
                  error_string = "Expected ( ) braces around blend factor with "
                                 "a subtraction";
                  goto error;
                }
              arg->factor.source.one_minus = TRUE;
              state = PARSER_ARG_STATE_EXPECT_COLOR_SRC_NAME;
            }
          else
            {
              arg->factor.is_one = TRUE;
              state = PARSER_ARG_STATE_EXPECT_CLOSE_PAREN;
            }
          continue;

        case PARSER_ARG_STATE_EXPECT_CLOSE_PAREN:
          if (implicit_factor_brace)
            {
              p--;
              state = PARSER_ARG_STATE_EXPECT_END;
              continue;
            }
          if (*p != ')')
            {
              error_string = "Expected closing parenthesis after blend factor";
              goto error;
            }
          state = PARSER_ARG_STATE_EXPECT_END;
          continue;

        case PARSER_ARG_STATE_MAYBE_MULT:
          if (*p == '*')
            {
              state = PARSER_ARG_STATE_EXPECT_OPEN_PAREN;
              continue;
            }
          arg->factor.is_one = TRUE;
          state = PARSER_ARG_STATE_EXPECT_END;

          /* fall through */
        case PARSER_ARG_STATE_EXPECT_END:
          if (*p != ',' && *p != ')')
            {
              error_string = "expected , or )";
              goto error;
            }

          *ret_p = p - 1;
          return TRUE;
        }
    }
  while (p++);

error:
  {
    int offset = p - string;
    _cogl_set_error (error,
                     COGL_BLEND_STRING_ERROR,
                     COGL_BLEND_STRING_ERROR_ARGUMENT_PARSE_ERROR,
                     "Syntax error for argument %d at offset %d: %s",
                     current_arg,
                     offset,
                     error_string);

    if (COGL_DEBUG_ENABLED (COGL_DEBUG_BLEND_STRINGS))
      {
        g_debug ("Syntax error for argument %d at offset %d: %s",
                 current_arg, offset, error_string);
      }
    return FALSE;
  }
}

int
_cogl_blend_string_compile (const char *string,
                            CoglBlendStringContext context,
                            CoglBlendStringStatement *statements,
                            CoglError **error)
{
  const char *p = string;
  const char *mark = NULL;
  const char *error_string;
  ParserState state = PARSER_STATE_EXPECT_DEST_CHANNELS;
  CoglBlendStringStatement *statement = statements;
  int current_statement = 0;
  int current_arg = 0;
  int remaining_argc = 0;

#if 0
  COGL_DEBUG_SET_FLAG (COGL_DEBUG_BLEND_STRINGS);
#endif

  if (COGL_DEBUG_ENABLED (COGL_DEBUG_BLEND_STRINGS))
    {
      COGL_NOTE (BLEND_STRINGS, "Compiling %s string:\n%s\n",
                 context == COGL_BLEND_STRING_CONTEXT_BLENDING ?
                 "blend" : "texture combine",
                 string);
    }

  do
    {
      if (g_ascii_isspace (*p))
        continue;

      if (*p == '\0')
        {
          switch (state)
            {
            case PARSER_STATE_EXPECT_DEST_CHANNELS:
              if (current_statement != 0)
                goto finished;
              error_string = "Empty statement";
              goto error;
            case PARSER_STATE_SCRAPING_DEST_CHANNELS:
              error_string = "Expected an '=' following the destination "
                "channel mask";
              goto error;
            case PARSER_STATE_EXPECT_FUNCTION_NAME:
              error_string = "Expected a function name";
              goto error;
            case PARSER_STATE_SCRAPING_FUNCTION_NAME:
              error_string = "Expected parenthesis after the function name";
              goto error;
            case PARSER_STATE_EXPECT_ARG_START:
              error_string = "Expected to find the start of an argument";
              goto error;
            case PARSER_STATE_EXPECT_STATEMENT_END:
              error_string = "Expected closing parenthesis for statement";
              goto error;
            }
        }

      switch (state)
        {
        case PARSER_STATE_EXPECT_DEST_CHANNELS:
          mark = p;
          state = PARSER_STATE_SCRAPING_DEST_CHANNELS;

          /* fall through */
        case PARSER_STATE_SCRAPING_DEST_CHANNELS:
          if (*p != '=')
            continue;
          if (strncmp (mark, "RGBA", 4) == 0)
            statement->mask = COGL_BLEND_STRING_CHANNEL_MASK_RGBA;
          else if (strncmp (mark, "RGB", 3) == 0)
            statement->mask = COGL_BLEND_STRING_CHANNEL_MASK_RGB;
          else if (strncmp (mark, "A", 1) == 0)
            statement->mask = COGL_BLEND_STRING_CHANNEL_MASK_ALPHA;
          else
            {
              error_string = "Unknown destination channel mask; "
                "expected RGBA=, RGB= or A=";
              goto error;
            }
          state = PARSER_STATE_EXPECT_FUNCTION_NAME;
          continue;

        case PARSER_STATE_EXPECT_FUNCTION_NAME:
          mark = p;
          state = PARSER_STATE_SCRAPING_FUNCTION_NAME;

          /* fall through */
        case PARSER_STATE_SCRAPING_FUNCTION_NAME:
          if (*p != '(')
            {
              if (!is_alphanum_char (*p))
                {
                  error_string = "non alpha numeric character in function"
                    "name";
                  goto error;
                }
              continue;
            }
          statement->function = get_function_info (mark, p, context);
          if (!statement->function)
            {
              error_string = "Unknown function name";
              goto error;
            }
          remaining_argc = statement->function->argc;
          current_arg = 0;
          state = PARSER_STATE_EXPECT_ARG_START;

          /* fall through */
        case PARSER_STATE_EXPECT_ARG_START:
          if (*p != '(' && *p != ',')
            continue;
          if (remaining_argc)
            {
              p++; /* parse_argument expects to see the first char of the arg */
              if (!parse_argument (string, &p, statement,
                                   current_arg, &statement->args[current_arg],
                                   context, error))
                return 0;
              current_arg++;
              remaining_argc--;
            }
          if (!remaining_argc)
            state = PARSER_STATE_EXPECT_STATEMENT_END;
          continue;

        case PARSER_STATE_EXPECT_STATEMENT_END:
          if (*p != ')')
            {
              error_string = "Expected end of statement";
              goto error;
            }
          state = PARSER_STATE_EXPECT_DEST_CHANNELS;
          if (current_statement++ == 1)
            goto finished;
          statement = &statements[current_statement];
        }
    }
  while (p++);

finished:

  if (COGL_DEBUG_ENABLED (COGL_DEBUG_BLEND_STRINGS))
    {
      if (current_statement > 0)
        print_statement (0, &statements[0]);
      if (current_statement > 1)
        print_statement (1, &statements[1]);
    }

  if (!validate_statements_for_context (statements,
                                        current_statement,
                                        context,
                                        error))
    return 0;

  return current_statement;

error:
    {
      int offset = p - string;
      _cogl_set_error (error,
                       COGL_BLEND_STRING_ERROR,
                       COGL_BLEND_STRING_ERROR_PARSE_ERROR,
                       "Syntax error at offset %d: %s",
                       offset,
                       error_string);

      if (COGL_DEBUG_ENABLED (COGL_DEBUG_BLEND_STRINGS))
        {
          g_debug ("Syntax error at offset %d: %s",
                   offset, error_string);
        }
      return 0;
    }
}

/*
 * INTERNAL TESTING CODE ...
 */

struct _TestString
{
  const char *string;
  CoglBlendStringContext context;
};

/* FIXME: this should probably be moved to a unit test */
int
_cogl_blend_string_test (void);

int
_cogl_blend_string_test (void)
{
  struct _TestString strings[] = {
        {"  A = MODULATE ( TEXTURE[RGB], PREVIOUS[A], PREVIOUS[A] )  ",
          COGL_BLEND_STRING_CONTEXT_TEXTURE_COMBINE },
        {"  RGB = MODULATE ( TEXTURE[RGB], PREVIOUS[A] )  ",
          COGL_BLEND_STRING_CONTEXT_TEXTURE_COMBINE },
        {"A=ADD(TEXTURE[A],PREVIOUS[RGB])",
          COGL_BLEND_STRING_CONTEXT_TEXTURE_COMBINE },
        {"A=ADD(TEXTURE[A],PREVIOUS[RGB])",
          COGL_BLEND_STRING_CONTEXT_TEXTURE_COMBINE },

        {"RGBA = ADD(SRC_COLOR*(SRC_COLOR[A]), DST_COLOR*(1-SRC_COLOR[A]))",
          COGL_BLEND_STRING_CONTEXT_BLENDING },
        {"RGB = ADD(SRC_COLOR, DST_COLOR*(0))",
          COGL_BLEND_STRING_CONTEXT_BLENDING },
        {"RGB = ADD(SRC_COLOR, 0)",
          COGL_BLEND_STRING_CONTEXT_BLENDING },
        {"RGB = ADD()",
          COGL_BLEND_STRING_CONTEXT_BLENDING },
        {"RGB = ADD(SRC_COLOR, 0, DST_COLOR)",
          COGL_BLEND_STRING_CONTEXT_BLENDING },
        {NULL}
  };
  int i;

  CoglError *error = NULL;
  for (i = 0; strings[i].string; i++)
    {
      CoglBlendStringStatement statements[2];
      int count = _cogl_blend_string_compile (strings[i].string,
                                              strings[i].context,
                                              statements,
                                              &error);
      if (!count)
        {
          g_print ("Failed to parse string:\n%s\n%s\n",
                   strings[i].string,
                   error->message);
          cogl_error_free (error);
          error = NULL;
          continue;
        }
      g_print ("Original:\n");
      g_print ("%s\n", strings[i].string);
      if (count > 0)
        print_statement (0, &statements[0]);
      if (count > 1)
        print_statement (1, &statements[1]);
    }

  return 0;
}

