#include "strv.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"

static char *str_dup(const char *s)
{
  ASSERT_RETURN(s, NULL);

  char *r = malloc(strlen(s) + 1);
  if (!r) {
    return NULL;
  }

  strcpy(r, s); // NOLINT

  return r;
}

char **strv_concat(char *const *a, const char *const *b)
{
  char *const *i = NULL;
  const char *const *j = NULL;
  size_t size = 1;
  size_t c = 0;

  STRV_FOREACH(i, a) {
    size++;
  }

  STRV_FOREACH(j, b) {
    size++;
  }

  char **r = calloc(size, sizeof(char *));
  if (!r) {
    goto finish;
  }

  STRV_FOREACH(i, a) {
    r[c] = str_dup(*i);
    if (!r[c]) {
      goto finish;
    }

    c++;
  }

  STRV_FOREACH(j, b) {
    r[c] = str_dup(*j);
    if (!r[c]) {
      goto finish;
    }

    c++;
  }

  r[c++] = NULL;

finish:
  if (c < size) {
    STRV_FOREACH(i, r) {
      free(*i);
    }

    free(r);

    return NULL;
  }

  return r;
}

char **strv_mergevar(char *const *a, const char *const *b)
{
    char *const *i = NULL;
    const char *const *j = NULL;
    size_t size = 1;
    size_t c = 0;
    
    STRV_FOREACH(i, a) {
        size++;
    }
    
    STRV_FOREACH(j, b) {
        bool merge = false;
        
        STRV_FOREACH(i, a) {
            if (
                strcspn(*i, "=") == strcspn(*j, "=") &&
                !strncasecmp(*i, *j, strcspn(*i, "="))
            ) {
                merge = true;
                break;
            }
        }
        
        if (merge == false) {
            size++;
        }
    }
    
    char **r = calloc(size, sizeof(char *));
    if (!r) {
        goto finish;
    }
    
    STRV_FOREACH(i, a) {
        const char *m = NULL;
        
        STRV_FOREACH(j, b) {
            if (
                strcspn(*i, "=") == strcspn(*j, "=") &&
                !strncasecmp(*i, *j, strcspn(*i, "="))
            ) {
                size_t len;
                
                m = *j + strcspn(*j, "=") + 1;
                len = strlen(m) + 1 + strlen(*i);
                r[c] = malloc(len + 1);
                if (!r[c]) {
                    goto finish;
                }
                r[c][0] = '\0';
                strcpy(r[c], *i);
                strcat(r[c], ":");
                strcat(r[c], m);
                
                break;
            }
        }
        
        if (m == NULL) {
            r[c] = str_dup(*i);
        }
        
        if (!r[c]) {
            goto finish;
        }
        
        c++;
    }
    
    STRV_FOREACH(j, b) {
        bool merged = false;
        
        STRV_FOREACH(i, r) {
            if (
                strcspn(*i, "=") == strcspn(*j, "=") &&
                !strncasecmp(*i, *j, strcspn(*i, "="))
            ) {
                merged = true;
                
                break;
            }
        }
        
        if (merged == false) {
            r[c] = str_dup(*j);
            if (!r[c]) {
                goto finish;
            }
            
            c++;
        }
    }
    
    r[c++] = NULL;
    
finish:
    if (c < size) {
        STRV_FOREACH(i, r) {
            free(*i);
        }
        
        free(r);
        
        return NULL;
    }
    
    return r;
}



char **strv_free(char **l)
{
  char **s = NULL;

  STRV_FOREACH(s, l) {
    free(*s);
  }

  free(l);

  return NULL;
}
