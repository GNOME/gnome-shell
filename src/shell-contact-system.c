/* This implements a complete suite for caching and searching contacts in the
 * Shell. We retrieve contacts from libfolks asynchronously and we search
 * these for display to the user. */

#include "shell-contact-system.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <gee.h>
#include <clutter/clutter.h>
#include <folks/folks.h>

#include "shell-global.h"
#include "shell-util.h"
#include "st.h"

G_DEFINE_TYPE (ShellContactSystem, shell_contact_system, G_TYPE_OBJECT);

#define ALIAS_PREFIX_MATCH_WEIGHT 100
#define ALIAS_SUBSTRING_MATCH_WEIGHT 90
#define IM_PREFIX_MATCH_WEIGHT 10
#define IM_SUBSTRING_MATCH_WEIGHT 5


/* Callbacks */

static void
prepare_individual_aggregator_cb (GObject       *obj,
                                  GAsyncResult  *res,
                                  gpointer      user_data)
{
  FolksIndividualAggregator *aggregator = FOLKS_INDIVIDUAL_AGGREGATOR (obj);

  folks_individual_aggregator_prepare_finish (aggregator, res, NULL);
}


/* Internal stuff */

typedef struct {
    gchar *key;
    guint weight;
} ContactSearchResult;

struct _ShellContactSystemPrivate {
    FolksIndividualAggregator *aggregator;
};

static void
shell_contact_system_constructed (GObject *obj)
{
  ShellContactSystem *self = SHELL_CONTACT_SYSTEM (obj);

  G_OBJECT_CLASS (shell_contact_system_parent_class)->constructed (obj);

  /* We intentionally do not care about the "individuals-changed" signal, as
   * we don't intend to update searches after they've been performed.
   * Therefore, we will simply retrieve the "individuals" property which
   * represents a snapshot of the individuals in the aggregator.
   */
  self->priv->aggregator = folks_individual_aggregator_new ();
  folks_individual_aggregator_prepare (self->priv->aggregator, prepare_individual_aggregator_cb, NULL);
}

static void
shell_contact_system_finalize (GObject *obj)
{
  ShellContactSystem *self = SHELL_CONTACT_SYSTEM (obj);

  g_object_unref (self->priv->aggregator);

  G_OBJECT_CLASS (shell_contact_system_parent_class)->finalize (obj);
}

static void
shell_contact_system_init (ShellContactSystem *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, SHELL_TYPE_CONTACT_SYSTEM, ShellContactSystemPrivate);
}

static void
shell_contact_system_class_init (ShellContactSystemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = shell_contact_system_constructed;
  object_class->finalize = shell_contact_system_finalize;

  g_type_class_add_private (object_class, sizeof (ShellContactSystemPrivate));
}

/**
 * normalize_terms:
 * @terms: (element-type utf8): Input search terms
 *
 * Returns: (element-type utf8) (transfer full): Unicode-normalized and lowercased terms
 */
static GSList *
normalize_terms (GSList *terms)
{
  GSList *normalized_terms = NULL;
  GSList *iter;
  for (iter = terms; iter; iter = iter->next)
    {
      const char *term = iter->data;
      normalized_terms = g_slist_prepend (normalized_terms, shell_util_normalize_and_casefold (term));
    }
  return normalized_terms;
}

static guint
do_match (ShellContactSystem  *self,
          FolksIndividual     *individual,
          GSList              *terms)
{
  GSList *term_iter;
  guint weight = 0;

  char *alias = shell_util_normalize_and_casefold (folks_alias_details_get_alias (FOLKS_ALIAS_DETAILS (individual)));

  GeeMultiMap *im_addr_map = folks_im_details_get_im_addresses (FOLKS_IM_DETAILS (individual));
  GeeCollection *im_addrs = gee_multi_map_get_values (im_addr_map);
  GeeIterator *im_addrs_iter;

  gboolean have_alias_prefix = FALSE;
  gboolean have_alias_substring = FALSE;
  
  gboolean have_im_prefix = FALSE;
  gboolean have_im_substring = FALSE;

  for (term_iter = terms; term_iter; term_iter = term_iter->next)
    {
      const char *term = term_iter->data;
      const char *p;

      /* Match on alias */
      if (alias != NULL)
	{
	  p = strstr (alias, term);
	  if (p == alias)
	    have_alias_prefix = TRUE;
	  else if (p != NULL)
	    have_alias_substring = TRUE;
	}

      /* Match on one or more IM addresses */
      im_addrs_iter = gee_iterable_iterator (GEE_ITERABLE (im_addrs));

      while (gee_iterator_next (im_addrs_iter))
        {
          const gchar *addr = gee_iterator_get (im_addrs_iter);

          p = strstr (addr, term);
          if (p == addr)
            have_im_prefix = TRUE;
          else if (p != NULL)
            have_im_substring = TRUE;
        }

      g_object_unref (im_addrs_iter);
    }

    if (have_alias_prefix)
      weight += ALIAS_PREFIX_MATCH_WEIGHT;
    else if (have_alias_substring)
      weight += ALIAS_SUBSTRING_MATCH_WEIGHT;

    if (have_im_prefix)
      weight += IM_PREFIX_MATCH_WEIGHT;
    else if (have_im_substring)
      weight += IM_SUBSTRING_MATCH_WEIGHT;

  g_free (alias);
  g_object_unref (im_addrs);

  return weight;
}

static gint
compare_results (gconstpointer a,
                 gconstpointer b)
{
  ContactSearchResult *first = (ContactSearchResult *) a;
  ContactSearchResult *second = (ContactSearchResult *) b;

  if (first->weight > second->weight)
      return 1;
  else if (first->weight < second->weight)
      return -1;
  else
      return 0;
}

static void
free_result (gpointer data,
             gpointer user_data)
{
  g_slice_free (ContactSearchResult, data);
}

/* modifies and frees @results */
static GSList *
sort_and_prepare_results (GSList *results)
{
  GSList *iter;
  GSList *sorted_results = NULL;

  results = g_slist_sort (results, compare_results);

  for (iter = results; iter; iter = iter->next)
    {
      ContactSearchResult *result = iter->data;
      gchar *id = result->key;
      sorted_results = g_slist_prepend (sorted_results, id);
    }

  g_slist_foreach (results, (GFunc) free_result, NULL);

  return sorted_results;
}


/* Methods */

/**
 * shell_contact_system_get_default:
 *
 * Return Value: (transfer none): The global #ShellContactSystem singleton
 */
ShellContactSystem *
shell_contact_system_get_default (void)
{
  static ShellContactSystem *instance = NULL;

  if (instance == NULL)
    instance = g_object_new (SHELL_TYPE_CONTACT_SYSTEM, NULL);

  return instance;
}

/**
 * shell_contact_system_get_all:
 * @self: A #ShellContactSystem
 *
 * Returns: (transfer none): All individuals
 */
GeeMap *
shell_contact_system_get_all (ShellContactSystem *self)
{
  GeeMap *individuals;

  g_return_val_if_fail (SHELL_IS_CONTACT_SYSTEM (self), NULL);

  individuals = folks_individual_aggregator_get_individuals (self->priv->aggregator);

  return individuals;
}

/**
 * shell_contact_system_get_individual:
 * @self: A #ShellContactSystem
 * @id: A #gchar with the ID of the FolksIndividual to be returned.
 *
 * Returns: (transfer full): A #FolksIndividual or NULL if @id could not be found.
 */
FolksIndividual *
shell_contact_system_get_individual (ShellContactSystem *self,
                                     gchar              *id)
{
  GeeMap *individuals;
  gpointer key, value;

  key = (gpointer) id;

  g_return_val_if_fail (SHELL_IS_CONTACT_SYSTEM (self), NULL);

  individuals = folks_individual_aggregator_get_individuals (self->priv->aggregator);

  value = gee_map_get (individuals, key);

  return FOLKS_INDIVIDUAL (value);
}

/**
 * shell_contact_system_initial_search:
 * @shell: A #ShellContactSystem
 * @terms: (element-type utf8): List of terms, logical AND
 *
 * Search through contacts for the given search terms.
 *
 * Returns: (transfer container) (element-type utf8): List of contact
 * identifiers
 */
GSList *
shell_contact_system_initial_search (ShellContactSystem *self,
                                     GSList             *terms)
{
  FolksIndividual *individual;
  GSList *results = NULL;
  GeeMap *individuals = NULL;
  ContactSearchResult *result;
  GeeMapIterator *iter;
  gpointer key;
  guint weight;
  GSList *normalized_terms = normalize_terms (terms);

  g_return_val_if_fail (SHELL_IS_CONTACT_SYSTEM (self), NULL);

  individuals = folks_individual_aggregator_get_individuals (self->priv->aggregator);

  iter = gee_map_map_iterator (individuals);

  while (gee_map_iterator_next (iter))
    {
      individual = gee_map_iterator_get_value (iter);
      weight = do_match (self, individual, normalized_terms);

      if (weight != 0)
        {
          key = gee_map_iterator_get_key (iter);

          result = g_slice_new (ContactSearchResult);
          result->key = (gchar *) key;
          result->weight = weight;

          results = g_slist_append (results, result);
        }

      g_object_unref (individual);
    }

  return sort_and_prepare_results (results);
}

/**
 * shell_contact_system_subsearch:
 * @shell: A #ShellContactSystem
 * @previous_results: (element-type utf8): List of previous results
 * @terms: (element-type utf8): List of terms, logical AND
 *
 * Search through a previous result set; for more information see
 * js/ui/search.js.
 *
 * Returns: (transfer container) (element-type utf8): List of contact
 * identifiers
 */
GSList *
shell_contact_system_subsearch (ShellContactSystem  *self,
                                GSList              *previous_results,
                                GSList              *terms)
{
  return shell_contact_system_initial_search (self, terms);
}
