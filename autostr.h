#ifndef AUTOSTR_H
#define AUTOSTR_H

/**
 * Initial size to allocate for each instance's char pointer, and size by which
 * to increase or decrease the allocated space when more or less is needed.
 */
#define AUTOSTR_CHUNK 256

/**
 * Structure that represents the autostr. All fields are private; use
 * the autostr_len and autostr_value functions instead.
 */
typedef struct {
    size_t size;
    size_t len;
    char *ptr;
} autostr_t;

/**
 * Allocate and return a new autostr instance.
 */
autostr_t *autostr();

/**
 * Deallocate the instance and set the pointer to NULL.
 */
void autostr_free(autostr_t **a);

/**
 * Get the instance's string.
 */
const char *autostr_value(autostr_t *a);

/**
 * Get the length of the string. This operation runs in O(1) time.
 */
int autostr_len(autostr_t *a);

/**
 * If instance points to a NULL pointer, assign it to a newly allocated
 * autostr. Otherwise, reset the existing instance to a blank string.
 */
autostr_t *autostr_recycle(autostr_t **a);

/**
 * Append a string to the instance.
 */
autostr_t *autostr_append(autostr_t *a, const char *append);

/**
 * Append a single char to the instance.
 */
autostr_t *autostr_push(autostr_t *a, char push);

/**
 * Trim whitespace from the start of the string.
 */
autostr_t *autostr_ltrim(autostr_t *a);

/**
 * Trim whitespace from the end of the string.
 */
autostr_t *autostr_rtrim(autostr_t *a);

/**
 * Trim whitespace from both sides of the string.
 */
autostr_t *autostr_trim(autostr_t *a);

/**
 * Compare the instance's value to the other value.
 */
int autostr_cmp(autostr_t *a, const char *other);

/**
 * Replace each character `c` in the string with the return value of func(c).
 * Providing tolower or toupper as the function yields the expected result.
 */
autostr_t *autostr_apply(autostr_t *a, int func(int));

#endif